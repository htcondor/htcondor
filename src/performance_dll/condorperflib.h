// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the TESTPERFLIB_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// TESTPERFLIB_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef CONDORPERFLIB_EXPORTS
#define CONDORPERFLIB_API __declspec(dllexport)
#else
#define CONDORPERFLIB_API __declspec(dllimport)
#endif
#pragma once
#include "countersdef.h"

extern "C" DWORD APIENTRY OpenPerfData(LPWSTR pContext);
extern "C" DWORD APIENTRY CollectPerfData(LPWSTR pQuery, PVOID* ppData, LPDWORD pcbData, LPDWORD pObjectsReturned);
extern "C" DWORD APIENTRY ClosePerfData();