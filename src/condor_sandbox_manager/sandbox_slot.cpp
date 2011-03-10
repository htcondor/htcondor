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

#include "sandbox_slot.h"


CSandboxSlot::CSandboxSlot(const char* sId, CSandbox* sb)
{
	printf("Entering CSandboxSlot::CSandboxSlot()\n");
	init(sId, sb);
}


// TODO: Do the cleanup of the sandbox slot data structure
CSandboxSlot::~CSandboxSlot()
{
	printf("CSandboxSlot::~CSandboxSlot called\n");
}


string
CSandboxSlot::getId(void)
{
	printf("CSandboxSlot::getId called\n");
	return this->id;
}


time_t
CSandboxSlot::getCreateTime(void)
{
	printf("CSandboxSlot::getCreateTime called\n");
	return this->sandbox->getCreateTime();
}


time_t
CSandboxSlot::getExpiry(void)
{
	printf("CSandboxSlot::getExpiry called\n");
	return this->sandbox->getExpiry();
}


bool
CSandboxSlot::isExpired(void)
{
	printf("CSandboxSlot::isExpired called\n");
	return this->sandbox->isExpired();
}


void
CSandboxSlot::renewExpiry(const int grace)
{
	printf("CSandboxSlot::renewExpiry called\n");
	this->sandbox->renewExpiry(grace);
}


void
CSandboxSlot::setExpiry(const int exp)
{
	printf("CSandboxSlot::setExpiry called\n");
	this->sandbox->setExpiry(exp);
}


string
CSandboxSlot::getDetails()
{
	printf("CSandboxSlot::getDetails called\n");
	std::ostringstream details;
	
	details	<< "id=" << this->id << std::endl
			<< "sandboxId=" << this->sandbox->getId() << std::endl
			<< "state=" << this->state << std::endl;

	return details.str();
}

/*****************************************************************************
* PRIVATE MEMBERS FUNCTIONS
*****************************************************************************/

void
CSandboxSlot::init(	const char* sId, CSandbox* sb)
{
	printf("CSandboxSlot::init called\n");

	// Set the id
	this->id.assign(sId);

	// Set the state of the sandbox
	this->state = SSS_UNUSED;

	// Set the sandbox pointer
	this->sandbox = sb;
}

