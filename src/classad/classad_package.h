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


#ifndef __CLASSAD_PACKAGE_H__
#define __CLASSAD_PACKAGE_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>

	// if this file is being included, it must be a standalone implementation
#define  STANDALONE

#include "classad/common.h"
#include "classad/exprTree.h"
#include "classad/matchClassad.h"
#include "classad/source.h"
#include "classad/sink.h"

#if defined(COLLECTIONS)
#include "collectionServer.h"
#include "collectionClient.h"
#endif

#endif
