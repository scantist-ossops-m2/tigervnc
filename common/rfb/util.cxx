/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2022 Pierre Ossman for Cendio AB
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
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>

#include <rfb/util.h>

namespace rfb {

  void CharArray::format(const char *fmt, ...) {
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    delete [] buf;

    if (len < 0) {
      buf = new char[1];
      buf[0] = '\0';
      return;
    }

    buf = new char[len+1];

    va_start(ap, fmt);
    vsnprintf(buf, len+1, fmt, ap);
    va_end(ap);
  }

  char* strDup(const char* s) {
    if (!s) return 0;
    int l = strlen(s);
    char* r = new char[l+1];
    memcpy(r, s, l+1);
    return r;
  };

  void strFree(char* s) {
    delete [] s;
  }


  bool strSplit(const char* src, const char limiter, char** out1, char** out2, bool fromEnd) {
    CharArray out1old, out2old;
    if (out1) out1old.buf = *out1;
    if (out2) out2old.buf = *out2;
    int len = strlen(src);
    int i=0, increment=1, limit=len;
    if (fromEnd) {
      i=len-1; increment = -1; limit = -1;
    }
    while (i!=limit) {
      if (src[i] == limiter) {
        if (out1) {
          *out1 = new char[i+1];
          if (i) memcpy(*out1, src, i);
          (*out1)[i] = 0;
        }
        if (out2) {
          *out2 = new char[len-i];
          if (len-i-1) memcpy(*out2, &src[i+1], len-i-1);
          (*out2)[len-i-1] = 0;
        }
        return true;
      }
      i+=increment;
    }
    if (out1) *out1 = strDup(src);
    if (out2) *out2 = 0;
    return false;
  }

  bool strContains(const char* src, char c) {
    int l=strlen(src);
    for (int i=0; i<l; i++)
      if (src[i] == c) return true;
    return false;
  }

  void strCopy(char* dest, const char* src, int destlen) {
    if (src)
      strncpy(dest, src, destlen-1);
    dest[src ? destlen-1 : 0] = 0;
  }

  static char intToHex(uint8_t i) {
    if (i<=9)
      return '0'+i;
    else if ((i>=10) && (i<=15))
      return 'a'+(i-10);
    assert(false);
    return '\0';
  }

  void binToHex(const uint8_t* in, size_t inlen,
                char* out, size_t outlen) {
    if (inlen > outlen/2)
      inlen = outlen/2;

    if (inlen > 0) {
      assert(in);
      assert(out);
    }

    for (size_t i=0; i<inlen; i++) {
      out[i*2] = intToHex((in[i] >> 4) & 15);
      out[i*2+1] = intToHex((in[i] & 15));
    }
  }

  std::string binToHex(const uint8_t* in, size_t inlen) {
    char* buffer = new char[inlen*2+1]();
    std::string out;
    binToHex(in, inlen, buffer, inlen*2);
    out = buffer;
    delete [] buffer;
    return out;
  }

  static bool readHexAndShift(char c, uint8_t* v) {
    c=tolower(c);
    if ((c >= '0') && (c <= '9'))
      *v = (*v << 4) + (c - '0');
    else if ((c >= 'a') && (c <= 'f'))
      *v = (*v << 4) + (c - 'a' + 10);
    else
      return false;
    return true;
  }

  bool hexToBin(const char* in, size_t inlen,
                uint8_t* out, size_t outlen) {
    assert(in);
    assert(out);

    if (inlen & 1)
      return false;

    if (inlen > outlen*2)
      inlen = outlen*2;

    for(size_t i=0; i<inlen; i+=2) {
      uint8_t byte = 0;
      if (!readHexAndShift(in[i], &byte) ||
          !readHexAndShift(in[i+1], &byte))
        return false;
      out[i/2] = byte;
    }

    return true;
  }

  std::vector<uint8_t> hexToBin(const char* in, size_t inlen) {
    std::vector<uint8_t> out(inlen/2);
    if (!hexToBin(in, inlen, out.data(), inlen/2))
      return std::vector<uint8_t>();
    return out;
  }

  std::string convertLF(const char* src, size_t bytes)
  {
    size_t sz;
    std::string out;

    const char* in;
    size_t in_len;

    // Compute output size
    sz = 0;
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      if (*in != '\r') {
        sz++;
        in++;
        in_len--;
        continue;
      }

      if ((in_len < 2) || (*(in+1) != '\n'))
        sz++;

      in++;
      in_len--;
    }

    // Reserve space
    out.reserve(sz);

    // And convert
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      if (*in != '\r') {
        out += *in++;
        in_len--;
        continue;
      }

      if ((in_len < 2) || (*(in+1) != '\n'))
        out += '\n';

      in++;
      in_len--;
    }

    return out;
  }

  std::string convertCRLF(const char* src, size_t bytes)
  {
    std::string out;
    size_t sz;

    const char* in;
    size_t in_len;

    // Compute output size
    sz = 0;
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      sz++;

      if (*in == '\r') {
        if ((in_len < 2) || (*(in+1) != '\n'))
          sz++;
      } else if (*in == '\n') {
        if ((in == src) || (*(in-1) != '\r'))
          sz++;
      }

      in++;
      in_len--;
    }

    // Reserve space
    out.reserve(sz);

    // And convert
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      if (*in == '\n') {
        if ((in == src) || (*(in-1) != '\r'))
          out += '\r';
      }

      out += *in;

      if (*in == '\r') {
        if ((in_len < 2) || (*(in+1) != '\n'))
          out += '\n';
      }

      in++;
      in_len--;
    }

    return out;
  }

  size_t ucs4ToUTF8(unsigned src, char dst[5]) {
    if (src < 0x80) {
      *dst++ = src;
      *dst++ = '\0';
      return 1;
    } else if (src < 0x800) {
      *dst++ = 0xc0 | (src >> 6);
      *dst++ = 0x80 | (src & 0x3f);
      *dst++ = '\0';
      return 2;
    } else if (src < 0x10000) {
      *dst++ = 0xe0 | (src >> 12);
      *dst++ = 0x80 | ((src >> 6) & 0x3f);
      *dst++ = 0x80 | (src & 0x3f);
      *dst++ = '\0';
      return 3;
    } else if (src < 0x110000) {
      *dst++ = 0xf0 | (src >> 18);
      *dst++ = 0x80 | ((src >> 12) & 0x3f);
      *dst++ = 0x80 | ((src >> 6) & 0x3f);
      *dst++ = 0x80 | (src & 0x3f);
      *dst++ = '\0';
      return 4;
    } else {
      return ucs4ToUTF8(0xfffd, dst);
    }
  }

  size_t utf8ToUCS4(const char* src, size_t max, unsigned* dst) {
    size_t count, consumed;

    *dst = 0xfffd;

    if (max == 0)
      return 0;

    consumed = 1;

    if ((*src & 0x80) == 0) {
      *dst = *src;
      count = 0;
    } else if ((*src & 0xe0) == 0xc0) {
      *dst = *src & 0x1f;
      count = 1;
    } else if ((*src & 0xf0) == 0xe0) {
      *dst = *src & 0x0f;
      count = 2;
    } else if ((*src & 0xf8) == 0xf0) {
      *dst = *src & 0x07;
      count = 3;
    } else {
      // Invalid sequence, consume all continuation characters
      src++;
      max--;
      while ((max-- > 0) && ((*src++ & 0xc0) == 0x80))
        consumed++;
      return consumed;
    }

    src++;
    max--;

    while (count--) {
      consumed++;

      // Invalid or truncated sequence?
      if ((max == 0) || ((*src & 0xc0) != 0x80)) {
        *dst = 0xfffd;
        return consumed;
      }

      *dst <<= 6;
      *dst |= *src & 0x3f;

      src++;
      max--;
    }

    return consumed;
  }

  size_t ucs4ToUTF16(unsigned src, wchar_t dst[3]) {
    if ((src < 0xd800) || ((src >= 0xe000) && (src < 0x10000))) {
      *dst++ = src;
      *dst++ = L'\0';
      return 1;
    } else if ((src >= 0x10000) && (src < 0x110000)) {
      src -= 0x10000;
      *dst++ = 0xd800 | ((src >> 10) & 0x03ff);
      *dst++ = 0xdc00 | (src & 0x03ff);
      *dst++ = L'\0';
      return 2;
    } else {
      return ucs4ToUTF16(0xfffd, dst);
    }
  }

  size_t utf16ToUCS4(const wchar_t* src, size_t max, unsigned* dst) {
    *dst = 0xfffd;

    if (max == 0)
      return 0;

    if ((*src < 0xd800) || (*src >= 0xe000)) {
      *dst = *src;
      return 1;
    }

    if (*src & 0x0400) {
      size_t consumed;

      // Invalid sequence, consume all continuation characters
      consumed = 0;
      while ((max > 0) && (*src & 0x0400)) {
        src++;
        max--;
        consumed++;
      }

      return consumed;
    }

    *dst = *src++;
    max--;

    // Invalid or truncated sequence?
    if ((max == 0) || ((*src & 0xfc00) != 0xdc00)) {
      *dst = 0xfffd;
      return 1;
    }

    *dst = 0x10000 + ((*dst & 0x03ff) << 10);
    *dst |= *src & 0x3ff;

    return 2;
  }

  std::string latin1ToUTF8(const char* src, size_t bytes) {
    std::string out;
    size_t sz;

    const char* in;
    size_t in_len;

    // Compute output size
    sz = 0;
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      char buf[5];
      sz += ucs4ToUTF8(*(const unsigned char*)in, buf);
      in++;
      in_len--;
    }

    // Reserve space
    out.reserve(sz);

    // And convert
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      char buf[5];
      ucs4ToUTF8(*(const unsigned char*)in, buf);
      out += buf;
      in++;
      in_len--;
    }

    return out;
  }

  std::string utf8ToLatin1(const char* src, size_t bytes) {
    std::string out;
    size_t sz;

    const char* in;
    size_t in_len;

    // Compute output size
    sz = 0;
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      size_t len;
      unsigned ucs;

      len = utf8ToUCS4(in, in_len, &ucs);
      in += len;
      in_len -= len;
      sz++;
    }

    // Reserve space
    out.reserve(sz);

    // And convert
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      size_t len;
      unsigned ucs;

      len = utf8ToUCS4(in, in_len, &ucs);
      in += len;
      in_len -= len;

      if (ucs > 0xff)
        out += '?';
      else
        out += (unsigned char)ucs;
    }

    return out;
  }

  std::string utf16ToUTF8(const wchar_t* src, size_t units)
  {
    std::string out;
    size_t sz;

    const wchar_t* in;
    size_t in_len;

    // Compute output size
    sz = 0;
    in = src;
    in_len = units;
    while ((in_len > 0) && (*in != '\0')) {
      size_t len;
      unsigned ucs;
      char buf[5];

      len = utf16ToUCS4(in, in_len, &ucs);
      in += len;
      in_len -= len;

      sz += ucs4ToUTF8(ucs, buf);
    }

    // Reserve space
    out.reserve(sz);

    // And convert
    in = src;
    in_len = units;
    while ((in_len > 0) && (*in != '\0')) {
      size_t len;
      unsigned ucs;
      char buf[5];

      len = utf16ToUCS4(in, in_len, &ucs);
      in += len;
      in_len -= len;

      ucs4ToUTF8(ucs, buf);
      out += buf;
    }

    return out;
  }

  std::wstring utf8ToUTF16(const char* src, size_t bytes)
  {
    std::wstring out;
    size_t sz;

    const char* in;
    size_t in_len;

    // Compute output size
    sz = 0;
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      size_t len;
      unsigned ucs;
      wchar_t buf[3];

      len = utf8ToUCS4(in, in_len, &ucs);
      in += len;
      in_len -= len;

      sz += ucs4ToUTF16(ucs, buf);
    }

    // Reserve space
    out.reserve(sz);

    // And convert
    in = src;
    in_len = bytes;
    while ((in_len > 0) && (*in != '\0')) {
      size_t len;
      unsigned ucs;
      wchar_t buf[3];

      len = utf8ToUCS4(in, in_len, &ucs);
      in += len;
      in_len -= len;

      ucs4ToUTF16(ucs, buf);
      out += buf;
    }

    return out;
  }

  unsigned msBetween(const struct timeval *first,
                     const struct timeval *second)
  {
    unsigned diff;

    diff = (second->tv_sec - first->tv_sec) * 1000;

    diff += second->tv_usec / 1000;
    diff -= first->tv_usec / 1000;

    return diff;
  }

  unsigned msSince(const struct timeval *then)
  {
    struct timeval now;

    gettimeofday(&now, NULL);

    return msBetween(then, &now);
  }

  bool isBefore(const struct timeval *first,
                const struct timeval *second)
  {
    if (first->tv_sec < second->tv_sec)
      return true;
    if (first->tv_sec > second->tv_sec)
      return false;
    if (first->tv_usec < second->tv_usec)
      return true;
    return false;
  }

  static std::string doPrefix(long long value, const char *unit,
                              unsigned divisor, const char **prefixes,
                              size_t prefixCount, int precision) {
    char buffer[256];
    double newValue;
    size_t prefix;

    newValue = value;
    prefix = 0;
    while (newValue >= divisor) {
      if (prefix >= prefixCount)
        break;
      newValue /= divisor;
      prefix++;
    }

    snprintf(buffer, sizeof(buffer), "%.*g %s%s", precision, newValue,
             (prefix == 0) ? "" : prefixes[prefix-1], unit);
    buffer[sizeof(buffer)-1] = '\0';

    return buffer;
  }

  static const char *siPrefixes[] =
    { "k", "M", "G", "T", "P", "E", "Z", "Y" };
  static const char *iecPrefixes[] =
    { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };

  std::string siPrefix(long long value, const char *unit,
                       int precision) {
    return doPrefix(value, unit, 1000, siPrefixes,
                    sizeof(siPrefixes)/sizeof(*siPrefixes),
                    precision);
  }

  std::string iecPrefix(long long value, const char *unit,
                        int precision) {
    return doPrefix(value, unit, 1024, iecPrefixes,
                    sizeof(iecPrefixes)/sizeof(*iecPrefixes),
                    precision);
  }
};
