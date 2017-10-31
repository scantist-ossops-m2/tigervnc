/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2021 Pierre Ossman for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <rfb/CMsgWriter.h>
#include <rfb/LogWriter.h>
#include <rfb/Exception.h>
#include <rfb/KeysymStr.h>
#include <rfb/ledStates.h>

// FLTK can pull in the X11 headers on some systems
#ifndef XK_VoidSymbol
#define XK_LATIN1
#define XK_MISCELLANY
#include <rfb/keysymdef.h>
#endif

#ifndef NoSymbol
#define NoSymbol 0
#endif

#include "Viewport.h"
#include "CConn.h"
#include "OptionsDialog.h"
#include "DesktopWindow.h"
#include "i18n.h"
#include "fltk_layout.h"
#include "parameters.h"
#include "vncviewer.h"

#include "PlatformPixelBuffer.h"

#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <FL/Fl_Menu.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/x.H>

#if defined(WIN32)
#include "KeyboardWin32.h"
#elif defined(__APPLE__)
#include "KeyboardMacOS.h"
#else
#include "KeyboardX11.h"
#endif

#ifdef __APPLE__
#include "cocoa.h"
#endif

using namespace rfb;
using namespace rdr;

static rfb::LogWriter vlog("Viewport");

// Menu constants

enum { ID_DISCONNECT, ID_FULLSCREEN, ID_MINIMIZE, ID_RESIZE,
       ID_CTRL, ID_ALT, ID_CTRLALTDEL,
       ID_REFRESH, ID_OPTIONS, ID_INFO, ID_ABOUT };

// Used for fake key presses from the menu
static const int FAKE_CTRL_KEY_CODE = 0x10001;
static const int FAKE_ALT_KEY_CODE = 0x10002;
static const int FAKE_DEL_KEY_CODE = 0x10003;

// Used for fake key presses for lock key sync
static const int FAKE_KEY_CODE = 0xffff;

Viewport::Viewport(int w, int h, const rfb::PixelFormat& serverPF, CConn* cc_)
  : Fl_Widget(0, 0, w, h), cc(cc_), frameBuffer(NULL),
    lastPointerPos(0, 0), lastButtonMask(0),
    keyboard(NULL), firstLEDState(true),
    pendingServerClipboard(false), pendingClientClipboard(false),
    menuCtrlKey(false), menuAltKey(false), cursor(NULL)
{
#if defined(WIN32)
  keyboard = new KeyboardWin32(this);
#elif defined(__APPLE__)
  keyboard = new KeyboardMacOS(this);
#else
  keyboard = new KeyboardX11(this);
#endif

  Fl::add_clipboard_notify(handleClipboardChange, this);

  // We need to intercept keyboard events early
  Fl::add_system_handler(handleSystemEvent, this);

  frameBuffer = new PlatformPixelBuffer(w, h);
  assert(frameBuffer);
  cc->setFramebuffer(frameBuffer);

  contextMenu = new Fl_Menu_Button(0, 0, 0, 0);
  // Setting box type to FL_NO_BOX prevents it from trying to draw the
  // button component (which we don't want)
  contextMenu->box(FL_NO_BOX);

  // The (invisible) button associated with this widget can mess with
  // things like Fl_Scroll so we need to get rid of any parents.
  // Unfortunately that's not possible because of STR #2654, but
  // reparenting to the current window works for most cases.
  window()->add(contextMenu);

  hotKeyHandler.setHotKeyCombo(hotKeyCombo);

  OptionsDialog::addCallback(handleOptions, this);

  // Make sure we have an initial blank cursor set
  setCursor(0, 0, rfb::Point(0, 0), NULL);
}


Viewport::~Viewport()
{
  // Unregister all timeouts in case they get a change tro trigger
  // again later when this object is already gone.
  Fl::remove_timeout(handlePointerTimeout, this);

  Fl::remove_system_handler(handleSystemEvent);

  Fl::remove_clipboard_notify(handleClipboardChange);

  OptionsDialog::removeCallback(handleOptions);

  if (cursor) {
    if (!cursor->alloc_array)
      delete [] cursor->array;
    delete cursor;
  }

  delete keyboard;

  // FLTK automatically deletes all child widgets, so we shouldn't touch
  // them ourselves here
}


const rfb::PixelFormat &Viewport::getPreferredPF()
{
  return frameBuffer->getPF();
}


// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.

void Viewport::updateWindow()
{
  Rect r;

  r = frameBuffer->getDamage();
  damage(FL_DAMAGE_USER1, r.tl.x + x(), r.tl.y + y(), r.width(), r.height());
}

static const char * dotcursor_xpm[] = {
  "5 5 2 1",
  ".	c #000000",
  " 	c #FFFFFF",
  "     ",
  " ... ",
  " ... ",
  " ... ",
  "     "};

void Viewport::setCursor(int width, int height, const Point& hotspot,
                         const rdr::U8* data)
{
  int i;

  if (cursor) {
    if (!cursor->alloc_array)
      delete [] cursor->array;
    delete cursor;
  }

  for (i = 0; i < width*height; i++)
    if (data[i*4 + 3] != 0) break;

  if ((i == width*height) && dotWhenNoCursor) {
    vlog.debug("cursor is empty - using dot");

    Fl_Pixmap pxm(dotcursor_xpm);
    cursor = new Fl_RGB_Image(&pxm);
    cursorHotspot.x = cursorHotspot.y = 2;
  } else {
    if ((width == 0) || (height == 0)) {
      U8 *buffer = new U8[4];
      memset(buffer, 0, 4);
      cursor = new Fl_RGB_Image(buffer, 1, 1, 4);
      cursorHotspot.x = cursorHotspot.y = 0;
    } else {
      U8 *buffer = new U8[width * height * 4];
      memcpy(buffer, data, width * height * 4);
      cursor = new Fl_RGB_Image(buffer, width, height, 4);
      cursorHotspot = hotspot;
    }
  }

  if (Fl::belowmouse() == this)
    window()->cursor(cursor, cursorHotspot.x, cursorHotspot.y);
}

void Viewport::handleClipboardRequest()
{
  Fl::paste(*this, clipboardSource);
}

void Viewport::handleClipboardAnnounce(bool available)
{
  if (!acceptClipboard)
    return;

  if (!available) {
    vlog.debug("Clipboard is no longer available on server");
    pendingServerClipboard = false;
    return;
  }

  pendingClientClipboard = false;

  if (!hasFocus()) {
    vlog.debug("Got notification of new clipboard on server whilst not focused, will request data later");
    pendingServerClipboard = true;
    return;
  }

  vlog.debug("Got notification of new clipboard on server, requesting data");
  cc->requestClipboard();
}

void Viewport::handleClipboardData(const char* data)
{
  size_t len;

  if (!hasFocus())
    return;

  len = strlen(data);

  vlog.debug("Got clipboard data (%d bytes)", (int)len);

  // RFB doesn't have separate selection and clipboard concepts, so we
  // dump the data into both variants.
#if !defined(WIN32) && !defined(__APPLE__)
  if (setPrimary)
    Fl::copy(data, len, 0);
#endif
  Fl::copy(data, len, 1);
}

void Viewport::setLEDState(unsigned int state)
{
  vlog.debug("Got server LED state: 0x%08x", state);

  // The first message is just considered to be the server announcing
  // support for this extension. We will push our state to sync up the
  // server when we get focus. If we already have focus we need to push
  // it here though.
  if (firstLEDState) {
    firstLEDState = false;
    if (hasFocus())
      pushLEDState();
    return;
  }

  if (!hasFocus())
    return;

  keyboard->setLEDState(state);
}

void Viewport::pushLEDState()
{
  unsigned int state;

  // Server support?
  if (cc->server.ledState() == ledUnknown)
    return;

  state = keyboard->getLEDState();
  if (state == ledUnknown)
    return;

#if defined(__APPLE__)
  // No support for Scroll Lock //
  state |= (cc->server.ledState() & ledScrollLock);
#endif

  if ((state & ledCapsLock) != (cc->server.ledState() & ledCapsLock)) {
    vlog.debug("Inserting fake CapsLock to get in sync with server");
    handleKeyPress(FAKE_KEY_CODE, 0x3a, XK_Caps_Lock);
    handleKeyRelease(FAKE_KEY_CODE);
  }
  if ((state & ledNumLock) != (cc->server.ledState() & ledNumLock)) {
    vlog.debug("Inserting fake NumLock to get in sync with server");
    handleKeyPress(FAKE_KEY_CODE, 0x45, XK_Num_Lock);
    handleKeyRelease(FAKE_KEY_CODE);
  }
  if ((state & ledScrollLock) != (cc->server.ledState() & ledScrollLock)) {
    vlog.debug("Inserting fake ScrollLock to get in sync with server");
    handleKeyPress(FAKE_KEY_CODE, 0x46, XK_Scroll_Lock);
    handleKeyRelease(FAKE_KEY_CODE);
  }
}


void Viewport::draw(Surface* dst)
{
  int X, Y, W, H;

  // Check what actually needs updating
  fl_clip_box(x(), y(), w(), h(), X, Y, W, H);
  if ((W == 0) || (H == 0))
    return;

  frameBuffer->draw(dst, X - x(), Y - y(), X, Y, W, H);
}


void Viewport::draw()
{
  int X, Y, W, H;

  // Check what actually needs updating
  fl_clip_box(x(), y(), w(), h(), X, Y, W, H);
  if ((W == 0) || (H == 0))
    return;

  frameBuffer->draw(X - x(), Y - y(), X, Y, W, H);
}


void Viewport::resize(int x, int y, int w, int h)
{
  if ((w != frameBuffer->width()) || (h != frameBuffer->height())) {
    vlog.debug("Resizing framebuffer from %dx%d to %dx%d",
               frameBuffer->width(), frameBuffer->height(), w, h);

    frameBuffer = new PlatformPixelBuffer(w, h);
    assert(frameBuffer);
    cc->setFramebuffer(frameBuffer);
  }

  Fl_Widget::resize(x, y, w, h);
}


int Viewport::handle(int event)
{
  char *filtered;
  int buttonMask, wheelMask;

  switch (event) {
  case FL_PASTE:
    filtered = convertLF(Fl::event_text(), Fl::event_length());

    vlog.debug("Sending clipboard data (%d bytes)", (int)strlen(filtered));

    try {
      cc->sendClipboardData(filtered);
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection_with_unexpected_error(e);
    }

    strFree(filtered);

    return 1;

  case FL_ENTER:
    window()->cursor(cursor, cursorHotspot.x, cursorHotspot.y);
    // Yes, we would like some pointer events please!
    return 1;

  case FL_LEAVE:
    window()->cursor(FL_CURSOR_DEFAULT);
    // We want a last move event to help trigger edge stuff
    handlePointerEvent(Point(Fl::event_x() - x(), Fl::event_y() - y()), 0);
    return 1;

  case FL_PUSH:
  case FL_RELEASE:
  case FL_DRAG:
  case FL_MOVE:
  case FL_MOUSEWHEEL:
    buttonMask = 0;
    if (Fl::event_button1())
      buttonMask |= 1;
    if (Fl::event_button2())
      buttonMask |= 2;
    if (Fl::event_button3())
      buttonMask |= 4;

    if (event == FL_MOUSEWHEEL) {
      wheelMask = 0;
      if (Fl::event_dy() < 0)
        wheelMask |= 8;
      if (Fl::event_dy() > 0)
        wheelMask |= 16;
      if (Fl::event_dx() < 0)
        wheelMask |= 32;
      if (Fl::event_dx() > 0)
        wheelMask |= 64;

      // A quick press of the wheel "button", followed by a immediate
      // release below
      handlePointerEvent(Point(Fl::event_x() - x(), Fl::event_y() - y()),
                         buttonMask | wheelMask);
    } 

    handlePointerEvent(Point(Fl::event_x() - x(), Fl::event_y() - y()), buttonMask);
    return 1;

  case FL_FOCUS:
    Fl::disable_im();

    flushPendingClipboard();

    // We may have gotten our lock keys out of sync with the server
    // whilst we didn't have focus. Try to sort this out.
    pushLEDState();

    // Resend Ctrl/Alt if needed
    if (menuCtrlKey)
      handleKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
    if (menuAltKey)
      handleKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);

    // Yes, we would like some focus please!
    return 1;

  case FL_UNFOCUS:
    // Release all keys that were pressed as that generally makes most
    // sense (e.g. Alt+Tab where we only see the Alt press)
    try {
      cc->releaseAllKeys();
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection(_("An unexpected error occurred when communicating "
                         "with the server:\n\n%s"), e.str());
    }
    keyboard->reset();
    hotKeyHandler.reset();
    Fl::enable_im();
    return 1;

  case FL_KEYDOWN:
  case FL_KEYUP:
    // Just ignore these as keys were handled in the event handler
    return 1;
  }

  return Fl_Widget::handle(event);
}

void Viewport::sendPointerEvent(const rfb::Point& pos, int buttonMask)
{
  if (viewOnly)
      return;

  if ((pointerEventInterval == 0) || (buttonMask != lastButtonMask)) {
    try {
      cc->writer()->writePointerEvent(pos, buttonMask);
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection_with_unexpected_error(e);
    }
  } else {
    if (!Fl::has_timeout(handlePointerTimeout, this))
      Fl::add_timeout((double)pointerEventInterval/1000.0,
                      handlePointerTimeout, this);
  }
  lastPointerPos = pos;
  lastButtonMask = buttonMask;
}

bool Viewport::hasFocus()
{
  Fl_Widget* focus;

  focus = Fl::grab();
  if (!focus)
    focus = Fl::focus();

  return focus == this;
}

void Viewport::handleClipboardChange(int source, void *data)
{
  Viewport *self = (Viewport *)data;

  assert(self);

  if (!sendClipboard)
    return;

#if !defined(WIN32) && !defined(__APPLE__)
  if (!sendPrimary && (source == 0))
    return;
#endif

  self->clipboardSource = source;

  self->pendingServerClipboard = false;

  if (!self->hasFocus()) {
    vlog.debug("Local clipboard changed whilst not focused, will notify server later");
    self->pendingClientClipboard = true;
    // Clear any older client clipboard from the server
    self->cc->announceClipboard(false);
    return;
  }

  vlog.debug("Local clipboard changed, notifying server");
  try {
    self->cc->announceClipboard(true);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_with_unexpected_error(e);
  }
}


void Viewport::flushPendingClipboard()
{
  if (pendingServerClipboard) {
    vlog.debug("Focus regained after remote clipboard change, requesting data");
    try {
      cc->requestClipboard();
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection_with_unexpected_error(e);
    }
  }
  if (pendingClientClipboard) {
    vlog.debug("Focus regained after local clipboard change, notifying server");
    try {
      cc->announceClipboard(true);
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection_with_unexpected_error(e);
    }
  }

  pendingServerClipboard = false;
  pendingClientClipboard = false;
}


void Viewport::handlePointerEvent(const rfb::Point& pos, int buttonMask)
{
  filterPointerEvent(pos, buttonMask);
}


void Viewport::handlePointerTimeout(void *data)
{
  Viewport *self = (Viewport *)data;

  assert(self);

  try {
    self->cc->writer()->writePointerEvent(self->lastPointerPos,
                                          self->lastButtonMask);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_with_unexpected_error(e);
  }
}


void Viewport::handleKeyPress(int systemKeyCode,
                              rdr::U32 keyCode, rdr::U32 keySym)
{
  HotKeyHandler::KeyAction action;

  // Possible hot key combo?

  action = hotKeyHandler.handleKeyPress(systemKeyCode, keySym);

  if (action == HotKeyHandler::KeyIgnore) {
    vlog.debug("Ignoring key press %d / 0x%04x / %s (0x%04x)",
               systemKeyCode, keyCode, KeySymName(keySym), keySym);
    return;
  }

  if (action == HotKeyHandler::KeyHotKey) {
    std::list<rdr::U32> keySyms;
    std::list<rdr::U32>::const_iterator iter;

    // Modifiers can change the KeySym that's been resolved, so we need
    // to check all possible KeySyms for this physical key, not just the
    // current one
    keySyms = keyboard->translateToKeySyms(systemKeyCode);

    // Then we pick the one that matches first
    keySym = NoSymbol;
    for (iter = keySyms.begin(); iter != keySyms.end(); iter++) {
      bool found;

      switch (*iter) {
      case XK_M:
      case XK_m:
        keySym = *iter;
        found = true;
        break;
      default:
        found = false;
        break;
      }

      if (found)
        break;
    }

    vlog.debug("Detected hot key %d / 0x%04x / %s (0x%04x)",
               systemKeyCode, keyCode, KeySymName(keySym), keySym);

    // The remote session won't see any more keys, so release the ones
    // currently down
    try {
      cc->releaseAllKeys();
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection(_("An unexpected error occurred when communicating "
                       "with the server:\n\n%s"), e.str());
    }

    switch (keySym) {
    case XK_M:
    case XK_m:
      popupContextMenu();
      break;
    default:
      // Unknown/Unused hot key combo
      break;
    }

    return;
  }

  // Normal key, so send to server...

  if (viewOnly)
    return;

  try {
    cc->sendKeyPress(systemKeyCode, keyCode, keySym);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_with_unexpected_error(e);
  }
}


void Viewport::handleKeyRelease(int systemKeyCode)
{
  HotKeyHandler::KeyAction action;

  // Possible hot key combo?

  action = hotKeyHandler.handleKeyRelease(systemKeyCode);

  if (action == HotKeyHandler::KeyIgnore) {
    vlog.debug("Ignoring key release %d", systemKeyCode);
    return;
  }

  if (action == HotKeyHandler::KeyHotKey) {
    vlog.debug("Hot key release %d", systemKeyCode);
    return;
  }

  // Normal key, so send to server...

  if (viewOnly)
    return;

  try {
    cc->sendKeyRelease(systemKeyCode);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_with_unexpected_error(e);
  }
}


int Viewport::handleSystemEvent(void *event, void *data)
{
  Viewport *self = (Viewport *)data;
  bool consumed;

  assert(self);

  if (!self->hasFocus())
    return 0;

  consumed = self->keyboard->handleEvent(event);
  if (consumed)
    return 1;

  return 0;
}

void Viewport::initContextMenu()
{
  contextMenu->clear();

  fltk_menu_add(contextMenu, p_("ContextMenu|", "Dis&connect"),
                0, NULL, (void*)ID_DISCONNECT, FL_MENU_DIVIDER);

  fltk_menu_add(contextMenu, p_("ContextMenu|", "&Full screen"),
                0, NULL, (void*)ID_FULLSCREEN,
                FL_MENU_TOGGLE | (window()->fullscreen_active()?FL_MENU_VALUE:0));
  fltk_menu_add(contextMenu, p_("ContextMenu|", "Minimi&ze"),
                0, NULL, (void*)ID_MINIMIZE, 0);
  fltk_menu_add(contextMenu, p_("ContextMenu|", "Resize &window to session"),
                0, NULL, (void*)ID_RESIZE,
                (window()->fullscreen_active()?FL_MENU_INACTIVE:0) |
                FL_MENU_DIVIDER);

  fltk_menu_add(contextMenu, p_("ContextMenu|", "&Ctrl"),
                0, NULL, (void*)ID_CTRL,
                FL_MENU_TOGGLE | (menuCtrlKey?FL_MENU_VALUE:0));
  fltk_menu_add(contextMenu, p_("ContextMenu|", "&Alt"),
                0, NULL, (void*)ID_ALT,
                FL_MENU_TOGGLE | (menuAltKey?FL_MENU_VALUE:0));

  fltk_menu_add(contextMenu, p_("ContextMenu|", "Send Ctrl-Alt-&Del"),
                0, NULL, (void*)ID_CTRLALTDEL, FL_MENU_DIVIDER);

  fltk_menu_add(contextMenu, p_("ContextMenu|", "&Refresh screen"),
                0, NULL, (void*)ID_REFRESH, FL_MENU_DIVIDER);

  fltk_menu_add(contextMenu, p_("ContextMenu|", "&Options..."),
                0, NULL, (void*)ID_OPTIONS, 0);
  fltk_menu_add(contextMenu, p_("ContextMenu|", "Connection &info..."),
                0, NULL, (void*)ID_INFO, 0);
  fltk_menu_add(contextMenu, p_("ContextMenu|", "About &TigerVNC viewer..."),
                0, NULL, (void*)ID_ABOUT, 0);
}


void Viewport::popupContextMenu()
{
  const Fl_Menu_Item *m;
  char buffer[1024];

  // Make sure the menu is reset to its initial state between goes or
  // it will start up highlighting the previously selected entry.
  contextMenu->value(-1);

  // initialize context menu before display
  initContextMenu();

  // Unfortunately FLTK doesn't reliably restore the mouse pointer for
  // menus, so we have to help it out.
  if (Fl::belowmouse() == this)
    window()->cursor(FL_CURSOR_DEFAULT);

  // FLTK also doesn't switch focus properly for menus
  handle(FL_UNFOCUS);

  m = contextMenu->popup();

  handle(FL_FOCUS);

  // Back to our proper mouse pointer.
  if (Fl::belowmouse())
    window()->cursor(cursor, cursorHotspot.x, cursorHotspot.y);

  if (m == NULL)
    return;

  switch (m->argument()) {
  case ID_DISCONNECT:
    disconnect();
    break;
  case ID_FULLSCREEN:
    if (window()->fullscreen_active())
      window()->fullscreen_off();
    else
      ((DesktopWindow*)window())->fullscreen_on();
    break;
  case ID_MINIMIZE:
    window()->iconize();
    break;
  case ID_RESIZE:
    if (window()->fullscreen_active())
      break;
    window()->size(w(), h());
    break;
  case ID_CTRL:
    if (m->value())
      handleKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
    else
      handleKeyRelease(FAKE_CTRL_KEY_CODE);
    menuCtrlKey = !menuCtrlKey;
    break;
  case ID_ALT:
    if (m->value())
      handleKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);
    else
      handleKeyRelease(FAKE_ALT_KEY_CODE);
    menuAltKey = !menuAltKey;
    break;
  case ID_CTRLALTDEL:
    handleKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
    handleKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);
    handleKeyPress(FAKE_DEL_KEY_CODE, 0xd3, XK_Delete);

    handleKeyRelease(FAKE_DEL_KEY_CODE);
    handleKeyRelease(FAKE_ALT_KEY_CODE);
    handleKeyRelease(FAKE_CTRL_KEY_CODE);
    break;
  case ID_REFRESH:
    cc->refreshFramebuffer();
    break;
  case ID_OPTIONS:
    OptionsDialog::showDialog();
    break;
  case ID_INFO:
    if (fltk_escape(cc->connectionInfo(), buffer, sizeof(buffer)) < sizeof(buffer)) {
      fl_message_title(_("VNC connection info"));
      fl_message("%s", buffer);
    }
    break;
  case ID_ABOUT:
    about_vncviewer();
    break;
  }
}

void Viewport::handleOptions(void *data)
{
  Viewport *self = (Viewport*)data;

  self->hotKeyHandler.setHotKeyCombo(hotKeyCombo);

  // FIXME: Need to recheck cursor for dotWhenNoCursor
}
