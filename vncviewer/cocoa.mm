/* Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/x.H>

#import <Cocoa/Cocoa.h>

#include <rfb/Rect.h>

static bool captured = false;

int cocoa_get_level(Fl_Window *win)
{
  NSWindow *nsw;
  nsw = (NSWindow*)fl_xid(win);
  return [nsw level];
}

void cocoa_set_level(Fl_Window *win, int level)
{
  NSWindow *nsw;
  nsw = (NSWindow*)fl_xid(win);
  [nsw setLevel:level];
}

int cocoa_capture_displays(Fl_Window *win)
{
  NSWindow *nsw;

  nsw = (NSWindow*)fl_xid(win);

  CGDisplayCount count;
  CGDirectDisplayID displays[16];

  int sx, sy, sw, sh;
  rfb::Rect windows_rect, screen_rect;

  windows_rect.setXYWH(win->x(), win->y(), win->w(), win->h());

  if (CGGetActiveDisplayList(16, displays, &count) != kCGErrorSuccess)
    return 1;

  if (count != (unsigned)Fl::screen_count())
    return 1;

  for (int i = 0; i < Fl::screen_count(); i++) {
    Fl::screen_xywh(sx, sy, sw, sh, i);

    screen_rect.setXYWH(sx, sy, sw, sh);
    if (screen_rect.enclosed_by(windows_rect)) {
      if (CGDisplayCapture(displays[i]) != kCGErrorSuccess)
        return 1;

    } else {
      // A display might have been captured with the previous
      // monitor selection. In that case we don't want to keep
      // it when its no longer inside the window_rect.
      CGDisplayRelease(displays[i]);
    }
  }

  captured = true;

  if ([nsw level] == CGShieldingWindowLevel())
    return 0;

  [nsw setLevel:CGShieldingWindowLevel()];

  return 0;
}

void cocoa_release_displays(Fl_Window *win)
{
  NSWindow *nsw;
  int newlevel;

  if (captured)
    CGReleaseAllDisplays();

  captured = false;

  nsw = (NSWindow*)fl_xid(win);

  // Someone else has already changed the level of this window
  if ([nsw level] != CGShieldingWindowLevel())
    return;

  // FIXME: Store the previous level somewhere so we don't have to hard
  //        code a level here.
  if (win->fullscreen_active() && win->contains(Fl::focus()))
    newlevel = NSStatusWindowLevel;
  else
    newlevel = NSNormalWindowLevel;

  // Only change if different as the level change also moves the window
  // to the top of that level.
  if ([nsw level] != newlevel)
    [nsw setLevel:newlevel];
}

CGColorSpaceRef cocoa_win_color_space(Fl_Window *win)
{
  NSWindow *nsw;
  NSColorSpace *nscs;

  nsw = (NSWindow*)fl_xid(win);

  nscs = [nsw colorSpace];
  if (nscs == nil) {
    // Offscreen, so return standard SRGB color space
    assert(false);
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  }

  CGColorSpaceRef lut = [nscs CGColorSpace];

  // We want a permanent reference, not an autorelease
  CGColorSpaceRetain(lut);

  return lut;
}

bool cocoa_win_is_zoomed(Fl_Window *win)
{
  NSWindow *nsw;
  nsw = (NSWindow*)fl_xid(win);
  return [nsw isZoomed];
}

void cocoa_win_zoom(Fl_Window *win)
{
  NSWindow *nsw;
  nsw = (NSWindow*)fl_xid(win);
  [nsw zoom:nsw];
}
