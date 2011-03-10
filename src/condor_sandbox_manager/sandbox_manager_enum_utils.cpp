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
#include "sandbox_manager_enum_utils.h"

using namespace std;


// TODO: This should move to some common header file. enum_utils.h ??

struct Translation SandboxStateTranslation[] = {
	{ "STAGEIN", SANDBOXSTATE_STAGEIN },
	{ "STAGEOUT", SANDBOXSTATE_STAGEOUT },
	{ "VALID", SANDBOXSTATE_VALID },
	{ "EXPIRED", SANDBOXSTATE_EXPIRED }
};


struct Translation SandboxSlotStateTranslation[] = {
	{ "UNUSED", SSS_UNUSED },
	{ "STAGEIN", SSS_STAGEIN },
	{ "STAGEOUT", SSS_STAGEOUT },
	{ "BOUND_TO_CPUSLOT", SSS_BOUND_TO_CPUSLOT },
	{ "EXPIRED", SSS_EXPIRED }
};

const char*
getSandboxStateString( SandboxState state )
{
	return getNameFromNum( (int)state, SandboxStateTranslation );
}


SandboxState
getSandboxStateNum( const char* str )
{
	return (SandboxState)getNumFromName( str, SandboxStateTranslation );
}

const char*
getSandboxSlotStateString( SandboxSlotState state )
{
	return getNameFromNum( (int)state, SandboxSlotStateTranslation );
}


SandboxSlotState 
getSandboxSlotStateNum( const char* str )
{
	return (SandboxSlotState)getNumFromName( str, SandboxSlotStateTranslation );
}
