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

#include "condor_common.h"

/*
HACK: These are the values for the GUIDs (globally unique identifiers
that are used in condor_mail and the windows_firewall COM code.

Normally, we'd link these in from uuid.lib, however, uuid.lib in the
XP SP2 SDK is "fundamentally incompatible" (says Microsoft support)
with the VC6 linker. All this, despite their XP SP2 SDK documentation
claiming in multiple places that the new SDK is compatible with both
.NET and VC6. Sigh.  So for now, we'll define them ourselves, and at our
earliest convenience move kicking and screaming into the world of .NET.

-stolley, Sept. 2004
*/

/** The following are needed in VC++ 6 for the Windows 
	Firewall and old condor_mail */
#if _MSC_VER == 1200
const IID IID_IEnumVARIANT = { 132100, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IPersistFile = { 267, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IStream = { 12, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IDispatch = { 132096, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IClassFactory = { 1, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IDropTarget = { 290, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IUnknown = { 0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
#endif
