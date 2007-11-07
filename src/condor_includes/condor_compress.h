/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef CONDOR_COMPRESS_H
#define CONDOR_COMPRESS_H

/*
This header describes the interface to compression code used by Condor.  Zlib is the general-purpose data compression library -- its interface and manual are all in zlib.h.  The GNU-zip data format is described in "algorithm.doc" in the gzip pacakage.  gzip does not come with any suitable header files, so structures and constants which match the format document are given below.
*/

#include "zlib.h"

#define GZIP_MAGIC0		0x1f
#define GZIP_MAGIC1		0x8b

#define GZIP_METHOD_STORED	0
#define GZIP_METHOD_COMPRESSED	1
#define GZIP_METHOD_PACKED	2
#define GZIP_METHOD_LZHED	3
#define GZIP_METHOD_DEFLATED	8

#define GZIP_FLAG_ASCII		0x01
#define GZIP_FLAG_MULTI		0x02
#define GZIP_FLAG_EXTRA		0x04
#define GZIP_FLAG_NAME		0x08
#define GZIP_FLAG_COMMENT	0x10
#define GZIP_FLAG_ENCRYPTED	0x20
#define GZIP_FLAG_RESERVED	0xc0

#define GZIP_SYSTEM_DOS		0x00
#define GZIP_SYSTEM_AMIGA	0x01
#define GZIP_SYSTEM_VMS		0x02
#define GZIP_SYSTEM_UNIX	0x03
#define GZIP_SYSTEM_ATARI	0x05
#define GZIP_SYSTEM_OS2		0x06
#define GZIP_SYSTEM_MAC		0x07
#define GZIP_SYSTEM_PRIMOS	0x0f
#define GZIP_SYSTEM_TOPS20	0x0a
#define GZIP_SYSTEM_WIN32	0x0a

struct gzip_header {
	unsigned char magic0, magic1;
	unsigned char method, flags;
	unsigned char time0, time1, time2, time3;
	unsigned char extra_flags, system;
};

#endif
