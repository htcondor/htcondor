/***************************************************************
Fermitools Software Legal Information (Modified BSD License)

COPYRIGHT STATUS: Dec 1st 2001, Fermi National Accelerator Laboratory (FNAL)
documents and software are sponsored by the U.S. Department of Energy under
Contract No. DE-AC02-76CH03000. Therefore, the U.S. Government retains a
world-wide non-exclusive, royalty-free license to publish or reproduce these
documents and software for U.S. Government purposes. All documents and
software available from this server are protected under the U.S. and Foreign
Copyright Laws, and FNAL reserves all rights.

    * Distribution of the software available from this server is free of
      charge subject to the user following the terms of the Fermitools
      Software Legal Information.

    * Redistribution and/or modification of the software shall be accompanied
      by the Fermitools Software Legal Information (including the copyright
      notice).

    * The user is asked to feed back problems, benefits, and/or suggestions
      about the software to the Fermilab Software Providers.

    * Neither the name of Fermilab, the FRA, nor the names of the contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

DISCLAIMER OF LIABILITY (BSD): THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL FERMILAB,
OR THE FRA, OR THE U.S. DEPARTMENT of ENERGY, OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Liabilities of the Government: This software is provided by FRA, independent
from its Prime Contract with the U.S. Department of Energy. FRA is acting
independently from the Government and in its own private capacity and is not
acting on behalf of the U.S. Government, nor as its contractor nor its agent.
Correspondingly, it is understood and agreed that the U.S. Government has no
connection to this software and in no manner whatsoever shall be liable for
nor assume any responsibility or obligation for any claim, cost, or damages
arising out of or resulting from the use of the software available from this
server.

Export Control: All documents and software available from this server are
subject to U.S. export control laws. Anyone downloading information from this
server is obligated to secure any necessary Government licenses before
exporting documents or software obtained from this server.
****************************************************************/

#include "condor_common.h"
#include "util_lib_proto.h"
#include "sandbox_manager.h"
#include "directory.h"
#include <string>
#include <iostream>
using namespace std;

CSandboxManager::CSandboxManager()
{
	// TODO: Fill in more details. Don't know yet
	cout << "CSandboxManager::CSandboxManager called" << std::endl;
	init();
}


CSandboxManager::~CSandboxManager()
{
	cout << "CSandboxManager::~CSandboxManager called" << std::endl;
	map<string, CSandbox*>::iterator iter;

	// Empty the map
	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		if (iter->second) {
			delete iter->second;
		}
	}
}


char*
CSandboxManager::registerSandbox(const char* sDir, const char* claimId, bool isId)
{
	dprintf(D_ALWAYS, "CSandboxManager::registerSandbox called \n");
	if (isId) {
		CSandbox *sandbox = new CSandbox(sDir, false);
		this->sandboxMap[sandbox->getId()] = sandbox;
		return (char*)sandbox->getId().c_str(); 
	}
	CSandbox *sandbox = new CSandbox(sDir);
	this->sandboxMap[sandbox->getId()] = sandbox;
	
	claimIdSandboxMap[string(claimId)].push_back(sandbox->getId());
	return (char*)sandbox->getId().c_str();
}


string
CSandboxManager::transferSandbox(const char* sid)
{
	dprintf(D_ALWAYS, "CSandboxManager::transferSandbox called for %s\n", sid);
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
	cout << "CSandboxManager::getExpiredSandboxIds called" << std::endl;
	time_t now;
	time(&now);
	vector<string> ids;
	map<string, CSandbox*>::iterator iter;

	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		cout	<< "Checking sandbox " << iter->second->getCreateTime()
				<< std::endl;
		if((unsigned int)now > (unsigned int)iter->second->getCreateTime()) {
			ids.push_back(iter->first);
		}
	}
	return ids;
}


void
CSandboxManager::listRegisteredSandboxes(void)
{
	cout << "CSandboxManager::listRegisteredSandboxes called" << std::endl;
	map<string, CSandbox*>::iterator iter;
	for (iter = sandboxMap.begin(); iter != sandboxMap.end(); ++iter) {
		std::cout	<< "DETAILS FOR: " << iter->first << std::endl
					<< (iter->second)->getDetails() << std::endl;
	}
}


void
CSandboxManager::unregisterSandbox(const char* id)
{
	cout	<< "CSandboxManager::listRegisteredSandboxes(char*) called" 
			<< std::endl;
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
			<< id << std::endl;
	// Find and delete the sandbox object
	iter = sandboxMap.find(id);
	if (iter->second) {
		delete iter->second;
	}
	sandboxMap.erase(id);
}


void
CSandboxManager::unregisterAllSandboxes(void)
{
	cout << "CSandboxManager::unregisterAllSandboxes called" << std::endl;
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

bool 
CSandboxManager::removeSandbox(std::string sandboxId) 
{
	if (this->sandboxMap.find(sandboxId) == this->sandboxMap.end())
		return true;
		
	CSandbox *sBox = this->sandboxMap[sandboxId];
	Directory *sBoxDir = new Directory(sBox->getSandboxDir().c_str());
	dprintf(D_FULLDEBUG, "CSandboxManager::removeSandbox: removing sandbox directory %s \n", sBox->getSandboxDir().c_str());
	bool result = sBoxDir->Remove_Entire_Directory();
	if (result) {
		result = sBoxDir->Remove_Full_Path(sBox->getSandboxDir().c_str());
	}
	delete sBoxDir;
	delete sBox;
	this->sandboxMap.erase(sandboxId);
	
	return result;
}

bool 
CSandboxManager::removeSandboxes(std::string claimId) 
{
	if (claimIdSandboxMap.find(claimId) == claimIdSandboxMap.end())
		return true;
		
	vector<std::string> sandboxIds = claimIdSandboxMap[claimId];
	int size = sandboxIds.size();
	bool result = true;
	bool interim = false;
	for (int i = 0; i < size; i++) {
		interim = removeSandbox(sandboxIds[i]);
		if (result) 
			result = interim;
		if (interim)
			dprintf(D_FULLDEBUG, "CSandboxManager::removeSandboxes: Removal of sandbox with id %s succeeded. \n", sandboxIds[i].c_str());
		else 
			dprintf(D_FULLDEBUG, "CSandboxManager::removeSandboxes: Removal of sandbox with id %s failed. \n", sandboxIds[i].c_str());
	}
}


/*****************************************************************************
* PRIVATE MEMBERS FUNCTIONS
*****************************************************************************/

void
CSandboxManager::init(void)
{
	cout << "CSandboxManager::init called" << std::endl;

	CSandboxManager::numSlotsTotal = 1;

    this->numSlotsActive = 0;
	this->numSlotsInactive = 0;
	this->numSlotsAvailable = CSandboxManager::numSlotsTotal;


	return;
}
