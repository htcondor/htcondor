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
#pragma once

#include <map>

class WindowsMessageReceiver;

class WindowsMessageReceiverOwner
{
public:
	virtual void onReceivedWindowsMessage(WindowsMessageReceiver *pSource, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};

class WindowsMessageReceiver
{
public:
	WindowsMessageReceiver(); 
	~WindowsMessageReceiver();

	void setOwner(WindowsMessageReceiverOwner *pOwner) { this->pOwner = pOwner; }
	HWND getHWnd() const;

private:
	HWND m_hWnd;
	WindowsMessageReceiverOwner *pOwner;

	void createHwnd();
	friend LRESULT CALLBACK WMR_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

std::map<HWND, WindowsMessageReceiver *> &mapWindowsMessageReceivers();
