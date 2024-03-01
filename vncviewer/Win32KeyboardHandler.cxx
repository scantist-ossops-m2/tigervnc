#include "Win32KeyboardHandler.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QCursor>
#include <QEvent>
#include <QResizeEvent>
#include <QTimer>
#include <qt_windows.h>
#include <windowsx.h>

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "rdr/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/keysymdef.h"
#include "rfb/ledStates.h"
#include "vncconnection.h"
#include "win32.h"

#include <QDebug>
#include <QMessageBox>
#include <QScreen>
#include <QTime>

static rfb::LogWriter vlog("Viewport");

// Used to detect fake input (0xaa is not a real key)
static const WORD SCAN_FAKE = 0xaa;
static const WORD NoSymbol  = 0;

Win32KeyboardHandler::Win32KeyboardHandler(QObject* parent) : BaseKeyboardHandler(parent)
{
    altGrCtrlTimer_.setInterval(100);
    altGrCtrlTimer_.setSingleShot(true);
    connect(&altGrCtrlTimer_, &QTimer::timeout, this, [=]() {
        altGrArmed_ = false;
        handleKeyPress(0x1d, XK_Control_L);
    });
}

bool Win32KeyboardHandler::nativeEventFilter(QByteArray const& eventType, void* message, long*)
{
    MSG* windowsmsg = static_cast<MSG*>(message);

    switch (windowsmsg->message)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        handleKeyDownEvent(windowsmsg->message, windowsmsg->wParam, windowsmsg->lParam);
        return true;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        handleKeyUpEvent(windowsmsg->message, windowsmsg->wParam, windowsmsg->lParam);
        return true;
    case WM_SETFOCUS:
        maybeGrabKeyboard();

        // We may have gotten our lock keys out of sync with the server
        // whilst we didn't have focus. Try to sort this out.
        pushLEDState();

        // Resend Ctrl/Alt if needed
        if (menuCtrlKey_)
        {
            handleKeyPress(0x1d, XK_Control_L);
        }
        if (menuAltKey_)
        {
            handleKeyPress(0x38, XK_Alt_L);
        }
        return true;
    case WM_KILLFOCUS:
        if (ViewerConfig::config()->fullscreenSystemKeys())
        {
            ungrabKeyboard();
        }
        // We won't get more key events, so reset our knowledge about keys
        resetKeyboard();
        return true;
    }

    return false;
}

void Win32KeyboardHandler::resolveAltGrDetection(bool isAltGrSequence)
{
    altGrArmed_ = false;
    altGrCtrlTimer_.stop();
    // when it's not an AltGr sequence we can't supress the Ctrl anymore
    if (!isAltGrSequence)
        handleKeyPress(0x1d, XK_Control_L);
}

void Win32KeyboardHandler::handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode)
{
    if (menuKeySym_ && keySym == menuKeySym_)
    {
        // if (isVisibleContextMenu())
        // {
        //     if (!menuShortCutMode)
        //     {
        //         sendContextMenuKey();
        //         return;
        //     }
        // }
        // else
        // {
        //     popupContextMenu();
        // }
        // return;
    }

    if (ViewerConfig::config()->viewOnly())
        return;

    if (keyCode == 0)
    {
        vlog.error(_("No key code specified on key press"));
        return;
    }

    // Because of the way keyboards work, we cannot expect to have the same
    // symbol on release as when pressed. This breaks the VNC protocol however,
    // so we need to keep track of what keysym a key _code_ generated on press
    // and send the same on release.
    downKeySym_[keyCode] = keySym;

    vlog.debug("Key pressed: 0x%04x => 0x%04x", keyCode, keySym);

    try
    {
        // Fake keycode?
        if (keyCode > 0xff)
            emit AppManager::instance()->connection()->writeKeyEvent(keySym, 0, true);
        else
            emit AppManager::instance()->connection()->writeKeyEvent(keySym, keyCode, true);
    }
    catch (rdr::Exception& e)
    {
        vlog.error("%s", e.str());
        AppManager::instance()->publishError(e.str(), true);
    }
}

void Win32KeyboardHandler::handleKeyRelease(int keyCode)
{
    DownMap::iterator iter;

    if (ViewerConfig::config()->viewOnly())
        return;

    iter = downKeySym_.find(keyCode);
    if (iter == downKeySym_.end())
    {
        // These occur somewhat frequently so let's not spam them unless
        // logging is turned up.
        vlog.debug("Unexpected release of key code %d", keyCode);
        return;
    }

    vlog.debug("Key released: 0x%04x => 0x%04x", keyCode, iter->second);

    try
    {
        if (keyCode > 0xff)
            emit AppManager::instance()->connection()->writeKeyEvent(iter->second, 0, false);
        else
            emit AppManager::instance()->connection()->writeKeyEvent(iter->second, keyCode, false);
    }
    catch (rdr::Exception& e)
    {
        vlog.error("%s", e.str());
        AppManager::instance()->publishError(e.str(), true);
    }

    downKeySym_.erase(iter);
}

int Win32KeyboardHandler::handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    Q_UNUSED(message);
    unsigned int timestamp  = GetMessageTime();
    UINT         vKey       = wParam;
    bool         isExtended = (lParam & (1 << 24)) != 0;
    int          keyCode    = ((lParam >> 16) & 0xff);

    // Windows' touch keyboard doesn't set a scan code for the Alt
    // portion of the AltGr sequence, so we need to help it out
    if (!isExtended && (keyCode == 0x00) && (vKey == VK_MENU))
    {
        isExtended = true;
        keyCode    = 0x38;
    }

    // Windows doesn't have a proper AltGr, but handles it using fake
    // Ctrl+Alt. However the remote end might not be Windows, so we need
    // to merge those in to a single AltGr event. We detect this case
    // by seeing the two key events directly after each other with a very
    // short time between them (<50ms) and supress the Ctrl event.
    if (altGrArmed_)
    {
        bool altPressed = isExtended && (keyCode == 0x38) && (vKey == VK_MENU) && ((timestamp - altGrCtrlTime_) < 50);
        resolveAltGrDetection(altPressed);
    }

    if (keyCode == SCAN_FAKE)
    {
        vlog.debug("Ignoring fake key press (virtual key 0x%02x)", vKey);
        return 1;
    }

    // Windows sets the scan code to 0x00 for multimedia keys, so we
    // have to do a reverse lookup based on the vKey.
    if (keyCode == 0x00)
    {
        keyCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
        if (keyCode == 0x00)
        {
            if (isExtended)
            {
                vlog.error(_("No scan code for extended virtual key 0x%02x"), (int)vKey);
            }
            else
            {
                vlog.error(_("No scan code for virtual key 0x%02x"), (int)vKey);
            }
            return 1;
        }
    }

    if (keyCode & ~0x7f)
    {
        vlog.error(_("Invalid scan code 0x%02x"), (int)keyCode);
        return 1;
    }

    if (isExtended)
    {
        keyCode |= 0x80;
    }

    // Fortunately RFB and Windows use the same scan code set (mostly),
    // so there is no conversion needed
    // (as long as we encode the extended keys with the high bit)

    // However Pause sends a code that conflicts with NumLock, so use
    // the code most RFB implementations use (part of the sequence for
    // Ctrl+Pause, i.e. Break)
    if (keyCode == 0x45)
    {
        keyCode = 0xc6;
    }
    // And NumLock incorrectly has the extended bit set
    if (keyCode == 0xc5)
    {
        keyCode = 0x45;
    }
    // And Alt+PrintScreen (i.e. SysRq) sends a different code than
    // PrintScreen
    if (keyCode == 0xb7)
    {
        keyCode = 0x54;
    }
    quint32 keySym = win32_vkey_to_keysym(vKey, isExtended);
    if (keySym == NoSymbol)
    {
        if (isExtended)
        {
            vlog.error(_("No symbol for extended virtual key 0x%02x"), (int)vKey);
        }
        else
        {
            vlog.error(_("No symbol for virtual key 0x%02x"), (int)vKey);
        }
    }

    // Windows sends the same vKey for both shifts, so we need to look
    // at the scan code to tell them apart
    if ((keySym == XK_Shift_L) && (keyCode == 0x36))
    {
        keySym = XK_Shift_R;
    }
    // AltGr handling (see above)
    if (win32_has_altgr())
    {
        if ((keyCode == 0xb8) && (keySym == XK_Alt_R))
        {
            keySym = XK_ISO_Level3_Shift;
        }
        // Possible start of AltGr sequence?
        if ((keyCode == 0x1d) && (keySym == XK_Control_L))
        {
            altGrArmed_    = true;
            altGrCtrlTime_ = timestamp;
            altGrCtrlTimer_.start();
            return 1;
        }
    }

    handleKeyPress(keyCode, keySym);

    // We don't get reliable WM_KEYUP for these
    switch (keySym)
    {
    case XK_Zenkaku_Hankaku:
    case XK_Eisu_toggle:
    case XK_Katakana:
    case XK_Hiragana:
    case XK_Romaji:
        handleKeyRelease(keyCode);
    }

    return 1;
}

int Win32KeyboardHandler::handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    Q_UNUSED(message);
    UINT vKey       = wParam;
    bool isExtended = (lParam & (1 << 24)) != 0;
    int  keyCode    = ((lParam >> 16) & 0xff);

    // Touch keyboard AltGr (see above)
    if (!isExtended && (keyCode == 0x00) && (vKey == VK_MENU))
    {
        isExtended = true;
        keyCode    = 0x38;
    }

    // We can't get a release in the middle of an AltGr sequence, so
    // abort that detection
    if (altGrArmed_)
    {
        resolveAltGrDetection(false);
    }
    if (keyCode == SCAN_FAKE)
    {
        vlog.debug("Ignoring fake key release (virtual key 0x%02x)", vKey);
        return 1;
    }

    if (keyCode == 0x00)
    {
        keyCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    }
    if (isExtended)
    {
        keyCode |= 0x80;
    }
    if (keyCode == 0x45)
    {
        keyCode = 0xc6;
    }
    if (keyCode == 0xc5)
    {
        keyCode = 0x45;
    }
    if (keyCode == 0xb7)
    {
        keyCode = 0x54;
    }

    handleKeyRelease(keyCode);

    // Windows has a rather nasty bug where it won't send key release
    // events for a Shift button if the other Shift is still pressed
    if ((keyCode == 0x2a) || (keyCode == 0x36))
    {
        if (downKeySym_.count(0x2a))
        {
            handleKeyRelease(0x2a);
        }
        if (downKeySym_.count(0x36))
        {
            handleKeyRelease(0x36);
        }
    }

    return 1;
}

void Win32KeyboardHandler::pushLEDState()
{
    qDebug() << "Win32KeyboardHandler::pushLEDState";
    // Server support?
    rfb::ServerParams* server = AppManager::instance()->connection()->server();
    if (server->ledState() == rfb::ledUnknown)
    {
        return;
    }

    unsigned int state = 0;
    if (GetKeyState(VK_CAPITAL) & 0x1)
    {
        state |= rfb::ledCapsLock;
    }
    if (GetKeyState(VK_NUMLOCK) & 0x1)
    {
        state |= rfb::ledNumLock;
    }
    if (GetKeyState(VK_SCROLL) & 0x1)
    {
        state |= rfb::ledScrollLock;
    }

    if ((state & rfb::ledCapsLock) != (server->ledState() & rfb::ledCapsLock))
    {
        vlog.debug("Inserting fake CapsLock to get in sync with server");
        handleKeyPress(0x3a, XK_Caps_Lock);
        handleKeyRelease(0x3a);
    }
    if ((state & rfb::ledNumLock) != (server->ledState() & rfb::ledNumLock))
    {
        vlog.debug("Inserting fake NumLock to get in sync with server");
        handleKeyPress(0x45, XK_Num_Lock);
        handleKeyRelease(0x45);
    }
    if ((state & rfb::ledScrollLock) != (server->ledState() & rfb::ledScrollLock))
    {
        vlog.debug("Inserting fake ScrollLock to get in sync with server");
        handleKeyPress(0x46, XK_Scroll_Lock);
        handleKeyRelease(0x46);
    }
}

void Win32KeyboardHandler::setLEDState(unsigned int state)
{
    qDebug() << "Win32KeyboardHandler::setLEDState";
    vlog.debug("Got server LED state: 0x%08x", state);

    // The first message is just considered to be the server announcing
    // support for this extension. We will push our state to sync up the
    // server when we get focus. If we already have focus we need to push
    // it here though.
    // if (firstLEDState_)
    // {
    //     firstLEDState_ = false;
    //     if (hasFocus())
    //     {
    //         pushLEDState();
    //     }
    //     return;
    // }

    // if (!hasFocus())
    // {
    //     return;
    // }

    INPUT input[6];
    memset(input, 0, sizeof(input));
    UINT count = 0;

    if (!!(state & rfb::ledCapsLock) != !!(GetKeyState(VK_CAPITAL) & 0x1))
    {
        input[count].type = input[count + 1].type = INPUT_KEYBOARD;
        input[count].ki.wVk = input[count + 1].ki.wVk = VK_CAPITAL;
        input[count].ki.wScan = input[count + 1].ki.wScan = SCAN_FAKE;
        input[count].ki.dwFlags                           = 0;
        input[count + 1].ki.dwFlags                       = KEYEVENTF_KEYUP;
        count += 2;
    }

    if (!!(state & rfb::ledNumLock) != !!(GetKeyState(VK_NUMLOCK) & 0x1))
    {
        input[count].type = input[count + 1].type = INPUT_KEYBOARD;
        input[count].ki.wVk = input[count + 1].ki.wVk = VK_NUMLOCK;
        input[count].ki.wScan = input[count + 1].ki.wScan = SCAN_FAKE;
        input[count].ki.dwFlags                           = KEYEVENTF_EXTENDEDKEY;
        input[count + 1].ki.dwFlags                       = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
        count += 2;
    }

    if (!!(state & rfb::ledScrollLock) != !!(GetKeyState(VK_SCROLL) & 0x1))
    {
        input[count].type = input[count + 1].type = INPUT_KEYBOARD;
        input[count].ki.wVk = input[count + 1].ki.wVk = VK_SCROLL;
        input[count].ki.wScan = input[count + 1].ki.wScan = SCAN_FAKE;
        input[count].ki.dwFlags                           = 0;
        input[count + 1].ki.dwFlags                       = KEYEVENTF_KEYUP;
        count += 2;
    }

    if (count == 0)
    {
        return;
    }

    UINT ret = SendInput(count, input, sizeof(*input));
    if (ret < count)
    {
        vlog.error(_("Failed to update keyboard LED state: %lu"), GetLastError());
    }
}

void Win32KeyboardHandler::grabKeyboard()
{
    BaseKeyboardHandler::grabKeyboard();
    // int ret = win32_enable_lowlevel_keyboard((HWND)winId());
    // if (ret != 0)
    // {
    //     vlog.error(_("Failure grabbing keyboard"));
    //     return;
    // }
}

void Win32KeyboardHandler::ungrabKeyboard()
{
    // win32_disable_lowlevel_keyboard((HWND)winId());
    BaseKeyboardHandler::ungrabKeyboard();
}