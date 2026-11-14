/*
 *  platform.h -- header file for platform specific parts.
 *
 *  Copyright (c) 2010 Georg Lukas <georg@op-co.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _PLATFORM_H
#define _PLATFORM_H

#ifdef _MSC_VER
  #define fseeko _fseeki64
#endif

// MinGW/GCC on Windows also needs the explicit 64-bit file-seek mapping:
// the default fseeko/ftello there are limited to 32-bit long offsets, which
// breaks MPQ archives larger than 4GB (e.g. HD-rework clients' common.MPQ).
// （MinGW/GCC 下默认 fseeko/ftello 偏移为 32 位 long，无法处理超过 4GB 的 MPQ
//   档案（如高清整合包客户端的 common.MPQ），同样需要显式映射 64 位 seek）
#if defined(_WIN32) && (defined(__MINGW32__) || defined(__MINGW64__))
  #ifndef fseeko
    #define fseeko(stream, offset, whence) _fseeki64((stream), (long long)(offset), (whence))
  #endif
  #ifndef ftello
    #define ftello(stream) _ftelli64((stream))
  #endif
#endif

#endif								/* _PLATFORM_H */
