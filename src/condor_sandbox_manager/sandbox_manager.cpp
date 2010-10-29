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

#include "sandbox_manager.h"
#include <string>
#include <iostream>
using namespace std;

CSandboxManager::CSandboxManager()
{
	// TODO: Fill in more details. Don't know yet
	printf("CSandboxManager::CSandboxManager called\n");
	init();
}


CSandboxManager::~CSandboxManager()
{
	printf("CSandboxManager::~CSandboxManager called\n");
	// TODO: Fill in more details. Don't know yet
}


char*
CSandboxManager::registerSandbox(const char* sDir)
{
	printf("CSandboxManager::registerSandbox called\n");
	CSandbox *sandbox = new CSandbox(sDir);
	this->sandboxMap[sandbox->getId()] = sandbox;
	return (char*)sandbox->getId().c_str();
}


int
CSandboxManager::transferSandbox(const char* sid)
{
	printf("CSandboxManager::transferSandbox called\n");
	return 1;
}


char*
CSandboxManager::getExpiredSandboxIds(void)
{
	printf("CSandboxManager::getExpiredSandboxIds called\n");
	return NULL;
}


void
CSandboxManager::unregisterSandbox(const char* id)
{
	return;
}


void
CSandboxManager::listRegisteredSandboxes(void)
{
	map<string, CSandbox*>::iterator iter;
	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		//printf("DETAILS FOR:%s\n%s\n", iter->first, "dd");
			//iter->first, (iter->second)->getDetails());
		std::cout << "DETAILS FOR: " << iter->first << std::endl
			  << (iter->second)->getDetails() << std::endl;
	}
}


/*****************************************************************************
* PRIVATE MEMBERS FUNCTIONS
*****************************************************************************/

void
CSandboxManager::init(void)
{
	printf("CSandboxManager::init called\n");
	// TODO: Fill in more details. Don't know yet
	return;
}
