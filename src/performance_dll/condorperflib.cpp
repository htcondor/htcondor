// testperflib.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "testperflib.h"
#include <Strsafe.h>
#include <Psapi.h>

//#pragma comment(linker, "/EXPORT:OpenPerfData=_OpenPerfData@4")
//#pragma comment(linker, "/EXPORT:CollectPerfData=_CollectPerfData@16")
//#pragma comment(linker, "/EXPORT:ClosePerfData=_ClosePerfData@0")

//Globals
PERF_OBJECT_TYPE condor_counter_obj;
PERF_COUNTER_DEFINITION *condor_counter_defs = NULL;
PERF_COUNTER_BLOCK condor_counter_block;
DWORD g_test_counter_index = 0;
DWORD g_queried_objects = 0;

wchar_t shared_stat_name[] = L"Global\\CondorStats";
HANDLE shared_stat_handle = NULL;

int *stat_buffer = NULL;

DWORD g_supported_counters[14];
DWORD counter_count;

DWORD perf_def_size = 0;
DWORD data_size = 0;

DWORD total_size = 0;
DWORD instances = 0;

HANDLE heapHandle;

DWORD APIENTRY OpenPerfData(LPWSTR pContext)
{
	HKEY hKey;
	DWORD dSize;
	DWORD first_counter_index = 0;
	DWORD first_help_index = 0;
	LONG rc = ERROR_SUCCESS;
	DWORD nameSize = MAX_PATH;
	wchar_t errormsg[MAX_PATH];

	dSize = sizeof(DWORD);
	heapHandle = GetProcessHeap();

	/*
	DWORD dRC;
	BOOL bRC;
	HANDLE pHandle;
	wchar_t pID[MAX_PATH];
	wchar_t pName[MAX_PATH];
	DWORD currentPID;
	pHandle = GetCurrentProcess();
	currentPID = GetCurrentProcessId();
	
	StringCchPrintf(pID, MAX_PATH, L"PID: %u\n", currentPID);
	OutputDebugString(pID);
	
	
	dRC = GetProcessImageFileName(pHandle, pName, nameSize);
	if(dRC)
	{
		OutputDebugString(pName);
		OutputDebugString(L"\n");
	}
	*/

	rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONDOR_SRV_REG, 0, KEY_READ, &hKey);
	if(rc == ERROR_SUCCESS)
	{
		rc = RegQueryValueEx(hKey, L"CounterCount", NULL, NULL, (LPBYTE)&(counter_count), &dSize);
		if(rc != ERROR_SUCCESS)
		{
			//error
		}

		RegCloseKey(hKey);
	}

	//data_size = counter_count * sizeof(int);
	data_size = sizeof(DWORD);
	condor_counter_block.ByteLength = data_size + sizeof(PERF_COUNTER_BLOCK);
	//perf_def_size = sizeof(PERF_COUNTER_DEFINITION) * counter_count;
	perf_def_size = sizeof(PERF_COUNTER_DEFINITION);

	shared_stat_handle = OpenFileMapping(
		FILE_MAP_READ,
		FALSE,
		shared_stat_name);

	if(shared_stat_handle != NULL)
	{
		stat_buffer = (int*)MapViewOfFile(
			shared_stat_handle,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			data_size);
	}


	//Implement processing of pContext later

	//Need to retrieve first index value from registry.
	rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, PERF_REG_PATH, 0, KEY_READ, &hKey);
	if(rc == ERROR_SUCCESS)
	{
		rc = RegQueryValueEx(hKey, L"First Counter", NULL, NULL, (LPBYTE)&first_counter_index, &dSize);
		if(rc == ERROR_SUCCESS)
		{
			dSize = sizeof(DWORD);
			rc = RegQueryValueEx(hKey, L"First Help", NULL, NULL, (LPBYTE)&first_help_index, &dSize);
		}
		else
		{
			StringCbPrintf(errormsg, MAX_PATH * sizeof(wchar_t), TEXT("Error code: %u\n"), rc);
			OutputDebugString(L"Failed to read first counter reg key\n");
			OutputDebugString(errormsg);
			return ERROR_SUCCESS;
		}

		RegCloseKey(hKey);
	}
	else
	{
		StringCbPrintf(errormsg, MAX_PATH * sizeof(wchar_t), TEXT("Error code: %u\n"), rc);
		OutputDebugString(L"Failed to open reg key\n");
		OutputDebugString(errormsg);
		return ERROR_SUCCESS;
	}

	if(rc != ERROR_SUCCESS)
	{
		StringCbPrintf(errormsg, MAX_PATH * sizeof(wchar_t), TEXT("Error code: %u\n"), rc);
		OutputDebugString(L"Failed to read first help reg key\n");
		OutputDebugString(errormsg);
		return ERROR_SUCCESS;
	}

	if(condor_counter_defs)
		HeapFree(heapHandle, 0, condor_counter_defs);

	condor_counter_defs = (PERF_COUNTER_DEFINITION*)HeapAlloc(heapHandle, 0, perf_def_size);

	//Initialize the counter object
	ZeroMemory(&condor_counter_obj, sizeof(PERF_OBJECT_TYPE));
	ZeroMemory(&condor_counter_defs, perf_def_size);
	condor_counter_obj.TotalByteLength = sizeof(PERF_OBJECT_TYPE) + perf_def_size + sizeof(PERF_COUNTER_BLOCK) + condor_counter_block.ByteLength;
	condor_counter_obj.DefinitionLength = sizeof(PERF_OBJECT_TYPE) + perf_def_size;
	condor_counter_obj.HeaderLength = sizeof(PERF_OBJECT_TYPE);
	condor_counter_obj.ObjectNameTitleIndex = first_counter_index;
	condor_counter_obj.ObjectHelpTitleIndex = first_help_index;
	condor_counter_obj.DetailLevel = PERF_DETAIL_ADVANCED;
	condor_counter_obj.NumCounters = counter_count;
	condor_counter_obj.DefaultCounter = -1;
	condor_counter_obj.NumInstances = PERF_NO_INSTANCES;

	//placeholder definition initialization for a single counter
	condor_counter_defs[0].ByteLength = sizeof(PERF_COUNTER_DEFINITION);
	condor_counter_defs[0].CounterNameTitleIndex = first_counter_index + 2;
	condor_counter_defs[0].CounterHelpTitleIndex = first_help_index + 2;
	condor_counter_defs[0].DetailLevel = PERF_DETAIL_ADVANCED;
	condor_counter_defs[0].CounterType = PERF_COUNTER_RAWCOUNT;
	condor_counter_defs[0].CounterSize = sizeof(int);
	condor_counter_defs[0].CounterOffset = sizeof(PERF_COUNTER_BLOCK) + sizeof(int);
	
	/*
	for(DWORD index = 0; index < counter_count; index++)
	{
		condor_counter_defs[index].ByteLength = sizeof(PERF_COUNTER_DEFINITION);
		condor_counter_defs[index].CounterNameTitleIndex = first_counter_index + 2 * (index + 1);
		condor_counter_defs[index].CounterHelpTitleIndex = first_help_index + 2 * (index + 1);
		condor_counter_defs[index].DetailLevel = PERF_DETAIL_ADVANCED;
		condor_counter_defs[index].CounterType = PERF_COUNTER_RAWCOUNT;
		condor_counter_defs[index].CounterSize = sizeof(int);
		condor_counter_defs[index].CounterOffset = sizeof(PERF_COUNTER_BLOCK) + index * sizeof(int);
	}
	*/
	
	return rc;
}

extern "C" DWORD APIENTRY CollectPerfData(LPWSTR pQuery, PVOID* ppData, LPDWORD pcbData, LPDWORD pObjectsReturned)
{
	bool querySupported = false;
	DWORD querySize = 0;

	*pObjectsReturned = 0;
	DWORD nameSize = MAX_PATH;
	
	

	/*
	DWORD dRC;
	BOOL bRC;
	DWORD currentPID;
	wchar_t pID[MAX_PATH];
	wchar_t pName[MAX_PATH];
	HANDLE pHandle;

	pHandle = GetCurrentProcess();
	currentPID = GetCurrentProcessId();
	
	StringCchPrintf(pID, MAX_PATH, L"PID: %u\n", currentPID);
	OutputDebugString(pID);

	dRC = GetProcessImageFileName(pHandle, pName, nameSize);
	if(dRC)
	{
		OutputDebugString(pName);
		OutputDebugString(L"\n");
	}
	*/
	querySupported = IsQuerySupported(pQuery);

	if(!querySupported)
	{
		OutputDebugString(L"Query is not supported.\n");
		return ERROR_SUCCESS;
	}

	if(*pcbData < condor_counter_obj.TotalByteLength)
	{
		return ERROR_MORE_DATA;
	}

	CopyMemory(pcbData, &condor_counter_obj, condor_counter_obj.HeaderLength);
	CopyMemory(((byte*)pcbData) + sizeof(PERF_OBJECT_TYPE), condor_counter_defs, perf_def_size);
	CopyMemory(((byte*)pcbData) + condor_counter_obj.DefinitionLength, &condor_counter_block, sizeof(PERF_COUNTER_BLOCK));
	if(stat_buffer)
	{
		CopyMemory(((byte*)pcbData) + condor_counter_obj.DefinitionLength + sizeof(PERF_COUNTER_BLOCK), stat_buffer, data_size);
	}
	else
	{
		CopyMemory(((byte*)pcbData) + condor_counter_obj.DefinitionLength + sizeof(PERF_COUNTER_BLOCK), &instances, data_size);
	}

	*pObjectsReturned = 1;

	return ERROR_SUCCESS;
}

extern "C" DWORD APIENTRY ClosePerfData()
{
	if(stat_buffer)
		UnmapViewOfFile(stat_buffer);
	if(shared_stat_handle)
		CloseHandle(shared_stat_handle);
	if(condor_counter_defs)
		HeapFree(heapHandle, 0, condor_counter_defs);

	return ERROR_SUCCESS;
}

bool IsQuerySupported(LPWSTR pQuery)
{
	LPWSTR pCopy = NULL;
	size_t queryLen = 0;
	wchar_t indexString[MAX_PATH + 1];

	queryLen = wcslen(pQuery) + 1;

	pCopy = new wchar_t[queryLen];
	wcscpy_s(pCopy, queryLen, pQuery);
	_wcslwr_s(pCopy, queryLen);

	if(wcsstr(pCopy, L"global"))
	{
		delete pCopy;
		return true;
	}
	else
	{
		//There are 13 counter indexes plus the index for the counter object.
		for(DWORD index = 0; index < counter_count + 1; index++)
		{
			int counter_index = condor_counter_obj.ObjectNameTitleIndex + 2 * index;
			_ultow_s(counter_index, indexString, MAX_PATH, 10);
			if(wcsstr(pCopy, indexString))
			{
				delete pCopy;

				return true;
			}
		}
	}

	delete pCopy;

	return false;
}