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


#if !defined(_CONDOR_SANDBOX_MANAGER_H)
#define _CONDOR_SANDBOX_MANAGER_H

//#include "condor_daemon_core.h"
//#include "list.h"

#include "sandbox.h"
#include <string>
#include <iostream>
#include <map>
#include <vector>
using namespace std;


/*
 The Condor Sandbox Manager class.
 It does some initialization and manages asynchronous sandbox transfers
*/
class CSandboxManager
{
public:
	// Constructor
	CSandboxManager();

	// Destructor
	virtual ~CSandboxManager();

	// Given the base dir, create a local are to move the sandbox, 
	// assign a sandboxId to the sandbox and set its expiry
	virtual char* registerSandbox(const char*, const char*, bool isId = false);
	
	virtual void updateSandboxExecDir(const char*, const char*);

	// Given the sandboxId give the handle to the sandbox location
	virtual string transferSandbox(const char*);

	// This is similar to Scheduler::requestSandboxLocation
	// We assume that the client sends request to do the transfers
	// The client tells the startd, which tells the sandbox manager
	// a bunch of jobs it would like to perform a transfer for out of 
	// a sandbox. The startd will hold open the connection back to the client
    // (potentially across to another callback) until it gets some information
    // from a transferd about the request and can give it back to the client
    // via callbacks
	// Maybe this should move to startd (??)
	//bool transferSandbox(int mode, Stream* s);

	// Return a list of ids containing expired sandboxes
	virtual std::vector<string> getExpiredSandboxIds(void);

	// List details about registered sandboxes
	virtual void listRegisteredSandboxes(void);

	// Unregister the sandbox with the given id
	virtual void unregisterSandbox(const char*);

	// Unregister the sandbox with the given id
	virtual void unregisterSandbox(const string);

	// Unregister all sandboxes
	virtual void unregisterAllSandboxes(void);
	
	// not sure whether we will need the following two functions in the long run, but for now it's useful.
	
	virtual void initIterator(void);
	
	virtual std::string getNextSandboxId(void);
	
	bool removeSandbox(std::string sandboxId);
	
	bool removeSandboxes(std::string claimId);

protected:


private:
	// Map of sandboxIds and a pointer to the sandbox object

	std::map<string, CSandbox*>sandboxMap;
	
	void init(void);
	
	std::map<std::string ,CSandbox*>::iterator m_iter;
	
	// a vector of sandbox ids associated with a particular claim
	std::map<std::string, vector<std::string> > claimIdSandboxMap;

};
#endif

