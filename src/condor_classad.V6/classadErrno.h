/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __CLASSAD_ERRNO_H__
#define __CLASSAD_ERRNO_H__

#include "common.h"

BEGIN_NAMESPACE(classad);

// bring in Condor system errors used if not in Condor
#if defined(CLASSAD_DISTRIBUTION)
static const int ERR_OK						= 0;
static const int ERR_MEM_ALLOC_FAILED		= 1;
#endif

static const int ERR_BAD_VALUE				= 255;
static const int ERR_FAILED_SET_VIEW_NAME 	= 256;
static const int ERR_NO_RANK_EXPR			= 257;
static const int ERR_NO_REQUIREMENTS_EXPR	= 258;
static const int ERR_BAD_PARTITION_EXPRS	= 259;
static const int ERR_PARTITION_EXISTS		= 260;
static const int ERR_MISSING_ATTRNAME		= 261;
static const int ERR_BAD_EXPRESSION			= 262;
static const int ERR_INVALID_IDENTIFIER		= 263;
static const int ERR_MISSING_ATTRIBUTE		= 264;
static const int ERR_NO_SUCH_VIEW			= 265;
static const int ERR_VIEW_PRESENT			= 266;
static const int ERR_TRANSACTION_EXISTS		= 267;
static const int ERR_NO_SUCH_TRANSACTION	= 268;
static const int ERR_NO_REPRESENTATIVE		= 269;
static const int ERR_NO_PARENT_VIEW			= 270;
static const int ERR_BAD_VIEW_INFO			= 271;
static const int ERR_BAD_TRANSACTION_STATE	= 272;
static const int ERR_NO_SUCH_CLASSAD		= 273;
static const int ERR_BAD_CLASSAD			= 275;
static const int ERR_NO_KEY					= 276;
static const int ERR_LOG_OPEN_FAILED		= 277;
static const int ERR_BAD_LOG_FILENAME		= 278;
static const int ERR_NO_VIEW_NAME			= 379;
static const int ERR_RENAME_FAILED			= 280;
static const int ERR_NO_TRANSACTION_NAME	= 281;
static const int ERR_PARSE_ERROR			= 282;
static const int ERR_INTERNAL_CACHE_ERROR	= 283;
static const int ERR_FILE_WRITE_FAILED		= 284;
static const int ERR_FATAL_ERROR			= 285;
static const int ERR_CANNOT_CHANGE_MODE		= 286;
static const int ERR_CONNECT_FAILED			= 287;
static const int ERR_CLIENT_NOT_CONNECTED	= 288;
static const int ERR_COMMUNICATION_ERROR	= 289;
static const int ERR_BAD_CONNECTION_TYPE	= 290;
static const int ERR_BAD_SERVER_ACK			= 291;
static const int ERR_CANNOT_REPLACE             =292;

static const int ERR_CACHE_SWITCH_ERROR         =293;
static const int ERR_CACHE_FILE_ERROR           =294;
END_NAMESPACE //classad

#endif
