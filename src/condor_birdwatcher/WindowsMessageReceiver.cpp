#include <stdafx.h>
#include <winbase.h>

#pragma warning(disable:4786)

#include "WindowsMessageReceiver.h"
#include <string>
using namespace std;

///////////////////////////////////////////////////////////////////////////////

map<HWND, WindowsMessageReceiver *> &mapWindowsMessageReceivers()
{
	static map<HWND, WindowsMessageReceiver *> mapWMRs;
	return mapWMRs;
}

///////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WMR_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WindowsMessageReceiver *pCurrentWMR = mapWindowsMessageReceivers()[hwnd];

	if (!pCurrentWMR)
	{
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	else 
	{
		if (pCurrentWMR->pOwner)
		{
			pCurrentWMR->pOwner->onReceivedWindowsMessage(pCurrentWMR, uMsg, wParam, lParam);
		}

		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

///////////////////////////////////////////////////////////////////////////////

WindowsMessageReceiver::WindowsMessageReceiver() : m_hWnd(NULL), pOwner(NULL)
{
	createHwnd();
}

///////////////////////////////////////////////////////////////////////////////

void WindowsMessageReceiver::createHwnd()
{
	static WNDCLASS	wc;
	static bool bFirstTime = true;
	static char pszClassNameBuf[256];

	srand((unsigned) (time(NULL)));

	HINSTANCE hinst = NULL;

	if (bFirstTime)
	{
		bFirstTime = false;

		wc.style					= CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc				= WMR_WindowProc;
		wc.cbClsExtra				= 0x0;
		wc.cbWndExtra				= sizeof(DWORD);
		wc.hInstance				= hinst;// NULL
		wc.hIcon					= NULL;
		wc.hCursor					= NULL;
		wc.hbrBackground			= (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszMenuName				= NULL;
		wc.lpszClassName			= pszClassNameBuf;

		strcpy(pszClassNameBuf, "WindowsMessageReceiver");
		
		int iTries = 0;
		while (!RegisterClass(&wc)) 
		{
			int iRand = rand()%10000;
			char sRand[32];
			itoa(iRand, sRand, 10);
			string strTemp = string("WindowsMessageReceiver") + sRand;
			
			strcpy(pszClassNameBuf, strTemp.c_str());
			
			iTries++;
			if (iTries > 10)
			{
				break;
			}
		}
	}
	
	m_hWnd = CreateWindow(pszClassNameBuf, "", 0, CW_USEDEFAULT, CW_USEDEFAULT, 1, 1, NULL,	NULL, hinst, NULL);
	mapWindowsMessageReceivers()[m_hWnd] = this;

	return;
}

///////////////////////////////////////////////////////////////////////////////

WindowsMessageReceiver::~WindowsMessageReceiver()
{
	DestroyWindow(m_hWnd);
	mapWindowsMessageReceivers().erase(m_hWnd);
}

///////////////////////////////////////////////////////////////////////////////

HWND WindowsMessageReceiver::getHWnd() const
{
	return m_hWnd;
}
