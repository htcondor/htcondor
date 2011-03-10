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

#include "sandbox.h"


//CSandbox::CSandbox(JobInfoCommunicator* my_jic, const char* sDir)
CSandbox::CSandbox(const char* sDir)
{
	printf("Entering CSandbox::CSandbox()\n");
	//init(my_jic, sDir, _MIN_LIFETIME, _MIN_SANDBOXID_LENGTH);
	init(sDir, _MIN_LIFETIME, _MIN_SANDBOXID_LENGTH);
}


//CSandbox::CSandbox(	JobInfoCommunicator* my_jic, const char* sDir,
CSandbox::CSandbox(	const char* sDir, const int lifeTime)
{
	printf("CSandbox::CSandbox called\n");
	//init(my_jic, sDir, lifeTime, _MIN_SANDBOXID_LENGTH);
	init(sDir, lifeTime, _MIN_SANDBOXID_LENGTH);
}


//CSandbox::CSandbox(	JobInfoCommunicator* my_jic, const char* sDir,
CSandbox::CSandbox(	const char* sDir, const int lifeTime, const int idLength)
{
	printf("CSandbox::CSandbox called\n");
	//init(my_jic, sDir, lifeTime, idLength);
	init(sDir, lifeTime, idLength);
}


// TODO: Do the cleanup of the sandbox data structure
CSandbox::~CSandbox()
{
	printf("CSandbox::~CSandbox called\n");
}


string
CSandbox::getId(void)
{
	printf("CSandbox::getId called\n");
	return this->id;
}


time_t
CSandbox::getCreateTime(void)
{
	printf("CSandbox::getCreateTime called\n");
	return this->createTime;
}


time_t
CSandbox::getExpiry(void)
{
	printf("CSandbox::getExpiry called\n");
	return this->expiry;
}


bool
CSandbox::isExpired(void)
{
	printf("CSandbox::isExpired called\n");
	time_t now;
	time(&now);
	bool expired = false;

	if (now >= this->expiry) {
		expired = true;
	}
	
	return expired;
	/*
	if (now >= this->expiry){
		this->state = SANDBOXSTATE_EXPIRED; 
	}
	return (this->state == SANDBOXSTATE_EXPIRED);
	*/
}


void
CSandbox::renewExpiry(const int grace)
{
	printf("CSandbox::renewExpiry called\n");
	time_t now;
	time(&now);
	this->expiry = now + grace;

	/*
	if (this->expiry > now) {
		this->state = SANDBOXSTATE_VALID;
	} else {
		this->state = SANDBOXSTATE_EXPIRED;
	}
	*/
}


void
CSandbox::setExpiry(const int exp)
{
	printf("CSandbox::setExpiry called\n");
	time_t now;
	time(&now);
	this->expiry = exp;

	/*
	if (this->expiry > now) {
		this->state = SANDBOXSTATE_VALID;
	} else {
		this->state = SANDBOXSTATE_EXPIRED;
	}
	*/
}


string
CSandbox::getSandboxDir(void)
{
	printf("CSandbox::getSandboxDir called\n");
	return this->sandboxDir;
}


void 
CSandbox::setSandboxDir(const char *sDir)
{
	this->sandboxDir = string(sDir);
}

string
CSandbox::getDetails()
{
	printf("CSandbox::getDetails called\n");
	std::ostringstream details;
	
	details	<< "id=" << this->id << std::endl
			<< "createTime=" << this->createTime << std::endl
			<< "expiry=" << this->expiry << std::endl
			<< "srcDir=" << this->srcDir << std::endl
			<< "sandboxDir=" << this->sandboxDir << std::endl
			<< "desintationURI=" << this->destinationURI << std::endl;
	return details.str();
}

/*****************************************************************************
* PRIVATE MEMBERS FUNCTIONS
*****************************************************************************/

void
//CSandbox::init(	JobInfoCommunicator* my_jic, const char* sDir, 
CSandbox::init(	const char* sDir, const int lifeTime, const int idLength)
{
	time_t now;

	printf("CSandbox::init called\n");
	// Create a unique id enforcing min id size limit
	this->id = (idLength >= this->_MIN_SANDBOXID_LENGTH) ?
		   createId(idLength) :
		   createId(_MIN_SANDBOXID_LENGTH);

	// Set the sandbox createTime and expiry
	time(&now);
	//asctime(localtime(&now));
	this->createTime = now;
	this->expiry = now + lifeTime;

	// Set the srcDir and the sandboxDir
	if (sDir) {
		this->srcDir.assign(sDir);
		// TODO: This may need to change or removed?
		this->sandboxDir.assign(this->srcDir);
	}

	// Set the state of the sandbox
	//this->state = SANDBOXSTATE_VALID;

	// Set the destinationURI
	this->destinationURI = "__NOT_SET__";

	//this->jic = my_jic;
}

