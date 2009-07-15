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

//#include <stdafx.h>
#include "systrayminimize.h"

#define DEFAULT_RECT_WIDTH 150
#define DEFAULT_RECT_HEIGHT 30

static VOID GetTrayWndRect(LPRECT lpTrayRect)
{
	HWND hShellTrayWnd=FindWindowEx(NULL,NULL,TEXT("Shell_TrayWnd"),NULL);
	if(hShellTrayWnd)
	{
		HWND hTrayNotifyWnd=FindWindowEx(hShellTrayWnd,NULL,TEXT("TrayNotifyWnd"),NULL);
		if(hTrayNotifyWnd)
		{
			GetWindowRect(hTrayNotifyWnd,lpTrayRect);
			return;
		}
	}
	
	APPBARDATA appBarData;
	appBarData.cbSize=sizeof(appBarData);
	if(SHAppBarMessage(ABM_GETTASKBARPOS,&appBarData))
	{
		// We know the edge the taskbar is connected to, so guess the rect of the
		// system tray. Use various fudge factor to make it look good
		switch(appBarData.uEdge)
		{
		case ABE_LEFT:
		case ABE_RIGHT:
			// We want to minimize to the bottom of the taskbar
			lpTrayRect->top=appBarData.rc.bottom-100;
			lpTrayRect->bottom=appBarData.rc.bottom-16;
			lpTrayRect->left=appBarData.rc.left;
			lpTrayRect->right=appBarData.rc.right;
			break;
			
		case ABE_TOP:
		case ABE_BOTTOM:
			// We want to minimize to the right of the taskbar
			lpTrayRect->top=appBarData.rc.top;
			lpTrayRect->bottom=appBarData.rc.bottom;
			lpTrayRect->left=appBarData.rc.right-100;
			lpTrayRect->right=appBarData.rc.right-16;
			break;
		}
		
		return;
	}
	
	hShellTrayWnd=FindWindowEx(NULL,NULL,TEXT("Shell_TrayWnd"),NULL);
	if(hShellTrayWnd)
	{
		GetWindowRect(hShellTrayWnd,lpTrayRect);
		if(lpTrayRect->right-lpTrayRect->left>DEFAULT_RECT_WIDTH)
			lpTrayRect->left=lpTrayRect->right-DEFAULT_RECT_WIDTH;
		if(lpTrayRect->bottom-lpTrayRect->top>DEFAULT_RECT_HEIGHT)
			lpTrayRect->top=lpTrayRect->bottom-DEFAULT_RECT_HEIGHT;
		
		return;
	}
	
	// OK. Haven't found a thing. Provide a default rect based on the current work
	// area
	SystemParametersInfo(SPI_GETWORKAREA,0,lpTrayRect,0);
	lpTrayRect->left=lpTrayRect->right-DEFAULT_RECT_WIDTH;
	lpTrayRect->top=lpTrayRect->bottom-DEFAULT_RECT_HEIGHT;
}

// Check to see if the animation has been disabled
static BOOL GetDoAnimateMinimize(VOID)
{
	ANIMATIONINFO ai;
	
	ai.cbSize=sizeof(ai);
	SystemParametersInfo(SPI_GETANIMATION,sizeof(ai),&ai,0);
	
	return ai.iMinAnimate?TRUE:FALSE;
}

VOID MinimizeWndToTray(HWND hWnd)
{
	if(GetDoAnimateMinimize())
	{
		RECT rcFrom,rcTo;
		
		// Get the rect of the window. It is safe to use the rect of the whole
		// window - DrawAnimatedRects will only draw the caption
		GetWindowRect(hWnd,&rcFrom);
		GetTrayWndRect(&rcTo);
		
		// Get the system to draw our animation for us
		DrawAnimatedRects(hWnd,IDANI_CAPTION,&rcFrom,&rcTo);
	}
	
	// Hide the window
	ShowWindow(hWnd,SW_HIDE);
}

VOID RestoreWndFromTray(HWND hWnd)
{
	
	if(GetDoAnimateMinimize())
	{
		// Get the rect of the tray and the window. Note that the window rect
		// is still valid even though the window is hidden
		RECT rcFrom,rcTo;
		GetTrayWndRect(&rcFrom);
		GetWindowRect(hWnd,&rcTo);
		
		// Get the system to draw our animation for us
		if(!DrawAnimatedRects(hWnd,IDANI_CAPTION,&rcFrom,&rcTo))
		{
			DWORD temp = GetLastError();
		WCHAR buffer[256];
		_ltow(temp, buffer, 10);
		OutputDebugString(buffer);
		}
	}
	
	// Show the window, and make sure we're the foreground window
	ShowWindow(hWnd,SW_SHOW);
	SetActiveWindow(hWnd);
	SetForegroundWindow(hWnd);
	
	// Remove the tray icon. As described above, remove the icon after the
	// call to DrawAnimatedRects, or the taskbar will not refresh itself
	// properly until DAR finished
}

