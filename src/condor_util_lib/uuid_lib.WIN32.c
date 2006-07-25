/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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

/* for the Firewall stuff */

#include "condor_common.h"

const IID IID_IEnumVARIANT = { 132100, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };

/* for condor_mail */
const IID IID_IDispatch = { 132096, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IPersistFile = { 267, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IClassFactory = { 1, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };

const IID IID_IDropTarget = { 290, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IStream = { 12, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
const IID IID_IUnknown = { 0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46} };
