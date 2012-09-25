/***************************************************************
*
* Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

// dllmain.cpp : Defines the entry point for the DLL application.
#include "hdfsJniHelper.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		//hdfsHashMutex = NULL;
		//jvmMutex = NULL;
		InitializeCriticalSection(&hdfsHashMutex);
		InitializeCriticalSection(&jvmMutex);
		tree = NULL;
		HASHSIZE = 0;
		count = 0;

		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&hdfsHashMutex);
		DeleteCriticalSection(&jvmMutex);
		break;
	}
	return TRUE;
}

int hcreate(size_t size)
{
	if(tree = (BUCK**)calloc(size, sizeof(BUCK*)))
	{
		HASHSIZE = size;
		count = 0;
		return 1;
	}

	return 0;
}

void hdestroy()
{
	BUCK *iterator;
	size_t index;
	BUCK *prev;
	if(tree)
	{
		for(index = 0; index < HASHSIZE; ++index)
		{
			iterator = tree[index];

			while(iterator)
			{
				free(iterator->entry.key);
				prev = iterator;
				iterator = iterator->next;
				free(prev);
			}
		}

		free(tree);
	}
}

ENTRY *hsearch(ENTRY e, enum ACTION a)
{
	unsigned int index;
	BUCK *bucket;
	BUCK *prev;
	char *k;

	index = hash(e.key);
	bucket = tree[index];
	if(!bucket)
	{
		if(a == ENTER)
		{
			if(count < HASHSIZE)
			{
				k = (char*)malloc(strlen(e.key));
				bucket = (BUCK*)malloc(sizeof(BUCK));
				strcpy(k, e.key);
				bucket->entry.key = k;
				bucket->entry.data = e.data;
				bucket->next = NULL;
				tree[index] = bucket;

				++count;
				return &(bucket->entry);
			}
		}

		return NULL;
	}
	else
	{
		while(bucket)
		{
			if(strcmp(bucket->entry.key, e.key))
				return &(bucket->entry);

			prev = bucket;
			bucket = bucket->next;
		}

		if(a == ENTER)
		{
			if(count < HASHSIZE)
			{
				k = (char*)malloc(strlen(e.key));
				bucket = (BUCK*)malloc(sizeof(BUCK));
				strcpy(k, e.key);
				bucket->entry.key = k;
				bucket->entry.data = e.data;
				bucket->next = NULL;
				prev->next = bucket;

				return &(bucket->entry);
			}
		}

		return NULL;
	}
}

unsigned int hash(const char *s)
{
	unsigned int hashval;

	for(hashval = 0; *s != '\0'; ++s)
		hashval = *s + 31 * hashval;

	return hashval % 101;
}
