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

#pragma once

#include <map>
#include <windows.h>
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
