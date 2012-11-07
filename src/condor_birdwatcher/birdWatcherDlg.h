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

#ifndef _watcherdlg_h_
#define _watcherdlg_h_
#if !defined(AFX_BIRDWATCHERDLG_H__2132927B_4970_46A0_A2D8_6F50BA17E681__INCLUDED_)
#define AFX_BIRDWATCHERDLG_H__2132927B_4970_46A0_A2D8_6F50BA17E681__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BirdWatcherDlg.h : header file
//
#include "birdwatcher.h"
/////////////////////////////////////////////////////////////////////////////
// CBirdWatcherDlg dialog
extern HWND parentHwnd;
extern TCHAR *zCondorDir;
INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnTimer(UINT nIDEvent);

#endif // !defined(AFX_BIRDWATCHERDLG_H__2132927B_4970_46A0_A2D8_6F50BA17E681__INCLUDED_)
#endif
