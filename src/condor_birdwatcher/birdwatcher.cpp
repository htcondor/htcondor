// condor_birdwatcher.cpp : Defines the entry point for the application.
//
#pragma once
#include "stdafx.h"
#include "birdwatcher.h"
#define MAX_LOADSTRING 100
HINSTANCE hInst;
// Global Variables:
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:

int APIENTRY wWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPWSTR    lpCmdLine,
                     int       nCmdShow)
{
	MSG msg;
	hInst = hInstance;

	SystrayManager sysman;
	HICON hFlying2 = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_FLYING2));
	HICON hClaimed = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_CLAIMED));

	sysman.init(LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_OFF)),
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_IDLE)),
				hClaimed,
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_FLYING1)),
				hFlying2,
				hFlying2,
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PREEMPTING))
				);
	
	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(birdwatcherDLG == 0 || !IsDialogMessage(birdwatcherDLG, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}
