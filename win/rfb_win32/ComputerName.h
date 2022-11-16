/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
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

#ifndef __RFB_WIN32_COMPUTERNAME_H__
#define __RFB_WIN32_COMPUTERNAME_H__

#include <windows.h>
#include <rfb_win32/TCharArray.h>

namespace rfb {
  namespace win32 {

    // Get the computer name
    struct ComputerName : core::TCharArray {
      ComputerName() : core::TCharArray(MAX_COMPUTERNAME_LENGTH+1) {
        ULONG namelength = MAX_COMPUTERNAME_LENGTH+1;
        if (!GetComputerName(buf, &namelength))
          _tcscpy(buf, _T(""));
      }
    };

  };
};

#endif
