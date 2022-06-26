/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2004 Red Hat Inc.
 * Copyright (C) 2010 TigerVNC Team
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

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <core/Exception.h>

using namespace core;

Exception::Exception(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	(void) vsnprintf(str_, len, format, ap);
	va_end(ap);
}

SystemException::SystemException(const char* s, int err_)
  : Exception("%s", s), err(err_)
{
  strncat(str_, ": ", len-1-strlen(str_));
#ifdef _WIN32
  wchar_t *currStr = new wchar_t[len-strlen(str_)];
  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 0, err, 0, currStr, len-1-strlen(str_), 0);
  WideCharToMultiByte(CP_UTF8, 0, currStr, -1, str_+strlen(str_),
                      len-1-strlen(str_), 0, 0);
  delete [] currStr;

  int l = strlen(str_);
  if ((l >= 2) && (str_[l-2] == '\r') && (str_[l-1] == '\n'))
      str_[l-2] = 0;

#else
  strncat(str_, strerror(err), len-1-strlen(str_));
#endif
  strncat(str_, " (", len-1-strlen(str_));
  char buf[20];
#ifdef WIN32
  if (err < 0)
    sprintf(buf, "%x", err);
  else
#endif
    sprintf(buf,"%d",err);
  strncat(str_, buf, len-1-strlen(str_));
  strncat(str_, ")", len-1-strlen(str_));
}
