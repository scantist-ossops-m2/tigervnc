/* Copyright 2016 Pierre Ossman for Cendio AB
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
#include <stdlib.h>
#include <QImage>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif
#include <rdr/Exception.h>
#include "appmanager.h"
#include "abstractvncview.h"
#include "Surface.h"

void Surface::clear(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
  if (!initialize()) {
    return;
  }
  XRenderColor color;

  color.red = (unsigned)r * 65535 / 255 * a / 255;
  color.green = (unsigned)g * 65535 / 255 * a / 255;
  color.blue = (unsigned)b * 65535 / 255 * a / 255;
  color.alpha = (unsigned)a * 65535 / 255;

  XRenderFillRectangle(m_display, PictOpSrc, m_picture, &color,
                       0, 0, width(), height());
}

void Surface::draw(int src_x, int src_y, int x, int y, int w, int h)
{
  if (!initialize()) {
    return;
  }
  Window window = (Window)AppManager::instance()->view()->nativeWindowHandle();
  Picture winPict = XRenderCreatePicture(m_display, window, m_visualFormat, 0, NULL);
  XRenderComposite(m_display, PictOpSrc, m_picture, None, winPict,
                   src_x, src_y, 0, 0, x, y, w, h);
  XRenderFreePicture(m_display, winPict);
}

void Surface::draw(Surface* dst, int src_x, int src_y, int x, int y, int w, int h)
{
  if (!initialize()) {
    return;
  }
  XRenderComposite(m_display, PictOpSrc, m_picture, None, dst->m_picture,
                   src_x, src_y, 0, 0, x, y, w, h);
}

Picture Surface::alpha_mask(int a)
{
  if (!initialize()) {
    return None;
  }
  Pixmap pixmap;
  XRenderPictFormat* format;
  XRenderPictureAttributes rep;
  Picture pict;
  XRenderColor color;

  if (a == 255)
    return None;

  Window window = (Window)AppManager::instance()->view()->nativeWindowHandle();
  pixmap = XCreatePixmap(m_display, window,
                         1, 1, 8);

  format = XRenderFindStandardFormat(m_display, PictStandardA8);
  rep.repeat = RepeatNormal;
  pict = XRenderCreatePicture(m_display, pixmap, format, CPRepeat, &rep);
  XFreePixmap(m_display, pixmap);

  color.alpha = (unsigned)a * 65535 / 255;

  XRenderFillRectangle(m_display, PictOpSrc, pict, &color,
                       0, 0, 1, 1);

  return pict;
}

void Surface::blend(int src_x, int src_y, int x, int y, int w, int h, int a)
{
  if (!initialize()) {
    return;
  }
  Window window = (Window)AppManager::instance()->view()->nativeWindowHandle();
  Picture winPict = XRenderCreatePicture(m_display, window, m_visualFormat, 0, NULL);
  Picture alpha = alpha_mask(a);
  XRenderComposite(m_display, PictOpOver, m_picture, alpha, winPict,
                   src_x, src_y, 0, 0, x, y, w, h);
  XRenderFreePicture(m_display, winPict);

  if (alpha != None)
    XRenderFreePicture(m_display, alpha);
}

void Surface::blend(Surface* dst, int src_x, int src_y, int x, int y, int w, int h, int a)
{
  if (!initialize()) {
    return;
  }
  Picture alpha;

  alpha = alpha_mask(a);
  XRenderComposite(m_display, PictOpOver, m_picture, alpha, dst->m_picture,
                   src_x, src_y, 0, 0, x, y, w, h);
  if (alpha != None)
    XRenderFreePicture(m_display, alpha);
}


void Surface::alloc()
{
  // Might not be open at this point
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  m_display = QX11Info::display();
#else
  m_display = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
}

bool Surface::initialize()
{
  if (m_visualFormat) {
    return true;
  }
  XRenderPictFormat templ;
  XRenderPictFormat* format;

  QAbstractVNCView *view = AppManager::instance()->view();
  if (!view) {
    return false;
  }
  Window window = (Window)view->nativeWindowHandle();
  if (!window) {
    return false;
  }
  int screen = XDefaultScreen(m_display);
  m_colorMap = XDefaultColormap(m_display, screen);
  m_gc = XDefaultGC(m_display, screen);

  XWindowAttributes xattr;
  XVisualInfo vtemplate;
  int nvinfo;
  XSync(m_display, False);
  Window rootWindow = DefaultRootWindow(m_display);
  XGetWindowAttributes(m_display, rootWindow, &xattr);
  vtemplate.screen = screen;
  vtemplate.visualid = XVisualIDFromVisual(xattr.visual);
  m_visualInfo = XGetVisualInfo(m_display, VisualNoMask, &vtemplate, &nvinfo);

  m_pixmap = XCreatePixmap(m_display, window,
                         width(), height(), 32);

  // Our code assumes a BGRA byte order, regardless of what the endian
  // of the machine is or the native byte order of XImage, so make sure
  // we find such a format
  templ.type = PictTypeDirect;
  templ.depth = 32;
  if (XImageByteOrder(m_display) == MSBFirst) {
    templ.direct.alpha = 0;
    templ.direct.red   = 8;
    templ.direct.green = 16;
    templ.direct.blue  = 24;
  } else {
    templ.direct.alpha = 24;
    templ.direct.red   = 16;
    templ.direct.green = 8;
    templ.direct.blue  = 0;
  }
  templ.direct.alphaMask = 0xff;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;

  format = XRenderFindFormat(m_display, PictFormatType | PictFormatDepth |
                             PictFormatRed | PictFormatRedMask |
                             PictFormatGreen | PictFormatGreenMask |
                             PictFormatBlue | PictFormatBlueMask |
                             PictFormatAlpha | PictFormatAlphaMask,
                             &templ, 0);

  if (!format)
    throw rdr::Exception("XRenderFindFormat");

  m_picture = XRenderCreatePicture(m_display, m_pixmap, format, 0, NULL);

  m_visualFormat = XRenderFindVisualFormat(m_display, m_visualInfo->visual);
  return true;
}

void Surface::dealloc()
{
  if (!m_picture) {
    XRenderFreePicture(m_display, m_picture);
  }
  if (!m_pixmap) {
    XFreePixmap(m_display, m_pixmap);
  }
  if (m_visualInfo) {
      XFree(m_visualInfo);
  }
}

void Surface::update(const QImage* image)
{
  if (!initialize()) {
    return;
  }
  XImage* img;
  GC gc;

  int x, y;
  const unsigned char* in;
  unsigned char* out;

  assert(image->width() == width());
  assert(image->height() == height());

  img = XCreateImage(m_display, CopyFromParent, 32,
                     ZPixmap, 0, NULL, width(), height(),
                     32, 0);
  if (!img)
    throw rdr::Exception("XCreateImage");

  img->data = (char*)malloc(img->bytes_per_line * img->height);
  if (!img->data)
    throw rdr::Exception("malloc");

  // Convert data and pre-multiply alpha
  in = image->constBits();
  out = (unsigned char*)img->data;
  for (y = 0;y < img->height;y++) {
    for (x = 0;x < img->width;x++) {
      switch ((image->depth() + 7) / 8) {
      case 1:
        *out++ = in[0];
        *out++ = in[0];
        *out++ = in[0];
        *out++ = 0xff;
        break;
      case 2:
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = in[1];
        break;
      case 3:
        *out++ = in[2];
        *out++ = in[1];
        *out++ = in[0];
        *out++ = 0xff;
        break;
      case 4:
        *out++ = (unsigned)in[2] * in[3] / 255;
        *out++ = (unsigned)in[1] * in[3] / 255;
        *out++ = (unsigned)in[0] * in[3] / 255;
        *out++ = in[3];
        break;
      }
      in += (image->depth() + 7) / 8;
    }

    // skip padding bytes, if any.
    if (image->bytesPerLine() != 0)
      in += image->bytesPerLine() - image->width() * (image->depth() + 7) / 8;
  }

  gc = XCreateGC(m_display, m_pixmap, 0, NULL);
  XPutImage(m_display, m_pixmap, gc, img,
            0, 0, 0, 0, img->width, img->height);
  XFreeGC(m_display, gc);

  XDestroyImage(img);
}
