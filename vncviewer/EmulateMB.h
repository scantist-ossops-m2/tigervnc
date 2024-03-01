/* Copyright 2020 Alex Tanskanen for Cendio AB
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

#ifndef __EMULATEMB__
#define __EMULATEMB__

#include <rfb/Rect.h>

class QTimer;

class EmulateMB
{
public:
  EmulateMB(QTimer* timer);

  void filterPointerEvent(rfb::Point const& pos, int buttonMask);

  void sendPointerEvent(rfb::Point const& pos, int buttonMask);

  virtual void handleTimeout();

  void sendAction(rfb::Point const& pos, int buttonMask, int action);

  int createButtonMask(int buttonMask);

private:
  int        state;
  int        emulatedButtonMask;
  int        lastButtonMask;
  rfb::Point lastPos, origPos;
  QTimer*    timer;
};

#endif
