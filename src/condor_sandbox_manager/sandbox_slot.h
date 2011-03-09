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


#if !defined(_CONDOR_SANDBOX_SLOT_H)
#define _CONDOR_SANDBOX_SLOT_H

#include "basename.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_state.h"
#include "condor_string.h"
#include "string_list.h"
#include "MyString.h"
#include "get_full_hostname.h"
#include "condor_random_num.h"
#include <string>

#include <sstream>

#include "sandbox.h"
#include "sandbox_manager_enum_utils.h"

using namespace std;

/*
 The Condor Sandbox class. This holds details about the actual sandbox
*/
class CSandboxSlot
{
public:
	// Constructors
	// Params: SlotId, pointer to the Sandbox
	CSandboxSlot(const char*, CSandbox*);

	// Destructor
	virtual ~CSandboxSlot();

	// Return my Id
	string getId(void);

	// Return the creation time
	time_t getCreateTime(void);

	// Return the expiry time
	time_t getExpiry(void);

	// Check if the sandbox is expired
	bool isExpired(void);

	// Extend the expiry by given secs
	void renewExpiry(const int);

	// Set the expiry to given time
	void setExpiry(const int);

	// Given the sandboxId give the handle to the sandbox location
	virtual string getDetails(void);
protected:


private:
	string id;				// Slot Id
	CSandbox *sandbox;		// Pointer to the sandbox held by this slot
	SandboxSlotState state;	// State sandbox is in
	// NOTE: Slot lease is determined by the expiry of the sandbox
	// For now we assume that once a sandbox gets the slot it will hold on 
	// to the slot till done.

	// Initialize the slot
	void init(const char*, CSandbox*);

};
#endif

