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

#ifndef __SURFACE_H__
#define __SURFACE_H__

#include <QByteArray>
#include <QDebug>
#include <QImage>

class Surface
{
public:
  Surface(int width, int height);

  void attach();
  void detach();

  int width()
  {
    return w;
  }

  int height()
  {
    return h;
  }

  QImage image()
  {
    attach();
    QImage i((unsigned char*)data.data(), w, h, QImage::Format_RGB32, imageCleanup, this);
    return i;
  }

  char* framebuffer()
  {
    return data.data();
  }

protected:
  ~Surface();

  static void imageCleanup(void* info)
  {
    Surface* self = (Surface*)info;
    self->detach();
  }

protected:
  int        w, h;
  QByteArray data;
  QAtomicInt refs;
};

#endif
