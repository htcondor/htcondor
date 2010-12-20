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

#include "condor_common.h"
#include "sandbox_manager.h"
#include "util_lib_proto.h"
#include <string>
#include <iostream>
using namespace std;

CSandboxManager::CSandboxManager()
{
	// TODO: Fill in more details. Don't know yet
	cout << "CSandboxManager::CSandboxManager called" << endl;
	init();
}


CSandboxManager::~CSandboxManager()
{
	cout << "CSandboxManager::~CSandboxManager called" << endl;
	map<string, CSandbox*>::iterator iter;

	// Empty the map
	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		if (iter->second) {
			delete iter->second;
		}
	}
	
}


char*
CSandboxManager::registerSandbox(const char* sDir)
{
	dprintf(D_ALWAYS, "CSandboxManager::registerSandbox called \n");
	cout << "CSandboxManager::registerSandbox called" << endl;
	CSandbox *sandbox = new CSandbox(sDir);
	this->sandboxMap[sandbox->getId()] = sandbox;
	return (char*)sandbox->getId().c_str();
}


string
CSandboxManager::transferSandbox(const char* sid)
{
	string s_sid(sid);
	if (sandboxMap.find(s_sid) != sandboxMap.end())
		return sandboxMap[s_sid]->getSandboxDir();
	return "(NULL)";
	//cout << "CSandboxManager::transferSandbox called" << endl;
	//return 1;
}


std::vector<string>
CSandboxManager::getExpiredSandboxIds(void)
{
	cout << "CSandboxManager::getExpiredSandboxIds called" << endl;
	time_t now;
	time(&now);
	vector<string> ids;
	map<string, CSandbox*>::iterator iter;

	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		cout << "Checking sandbox " << iter->second->getCreateTime() << endl;
		if((unsigned int)now > (unsigned int)iter->second->getCreateTime()) {
			ids.push_back(iter->first);
		}
	}
	return ids;
}


void
CSandboxManager::listRegisteredSandboxes(void)
{
	cout << "CSandboxManager::listRegisteredSandboxes called" << endl;
	map<string, CSandbox*>::iterator iter;
	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		std::cout << "DETAILS FOR: " << iter->first << std::endl
			  << (iter->second)->getDetails() << std::endl;
	}
}


void
CSandboxManager::unregisterSandbox(const char* id)
{
	cout << "CSandboxManager::listRegisteredSandboxes(char*) called" 
		<< endl;
	string ids;
	ids.assign(id);
	this->unregisterSandbox(ids);
}


void
CSandboxManager::unregisterSandbox(const string id)
{
	// TODO: Need more details. Right now just delete the sandbox object
	map<string, CSandbox*>::iterator iter;
	cout 	<< "CSandboxManager::listRegisteredSandboxes called for " 
		<< id << endl;
	// Find and delete the sandbox object
	iter = sandboxMap.find(id);
	delete iter->second;
	sandboxMap.erase(id);
}


void
CSandboxManager::unregisterAllSandboxes(void)
{
	cout << "CSandboxManager::unregisterAllSandboxes called" << endl;
	map<string, CSandbox*>::iterator iter;
	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		this->unregisterSandbox(iter->first);
	}
}

////////////////

void 
CSandboxManager::initIterator(void)
{
	this->m_iter = this->sandboxMap.begin();
}
	
std::string 
CSandboxManager::getNextSandboxId(void) 
{
	this->m_iter++;
	if (this->m_iter == this->sandboxMap.end())
		return "";
	return this->m_iter->first;
	

}


/*****************************************************************************
* PRIVATE MEMBERS FUNCTIONS
*****************************************************************************/

void
CSandboxManager::init(void)
{
	cout << "CSandboxManager::init called" << endl;
	// TODO: Fill in more details. Don't know yet
	return;
}
