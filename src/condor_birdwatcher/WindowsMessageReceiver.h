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
