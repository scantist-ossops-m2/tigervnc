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

#include <core/Timer.h>
#include <core/Rect.h>

class EmulateMB : public core::Timer::Callback {
public:
  EmulateMB();

  void filterPointerEvent(const core::Point& pos, int buttonMask);

protected:
  virtual void sendPointerEvent(const core::Point& pos, int buttonMask)=0;

  virtual bool handleTimeout(core::Timer *t);

private:
  void sendAction(const core::Point& pos, int buttonMask, int action);

  int createButtonMask(int buttonMask);

private:
  int state;
  int emulatedButtonMask;
  int lastButtonMask;
  core::Point lastPos, origPos;
  core::Timer timer;
};

#endif
