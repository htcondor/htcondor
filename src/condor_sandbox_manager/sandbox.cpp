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

#include "sandbox.h"


CSandbox::CSandbox(const char* sDir)
{
	printf("CSandbox::CSandbox called\n");
	init(sDir, _MIN_LIFETIME, _MIN_SANDBOXID_LENGTH);
}


CSandbox::CSandbox(const char* sDir, const int lifeTime)
{
	printf("CSandbox::CSandbox called\n");
	init(sDir, lifeTime, _MIN_SANDBOXID_LENGTH);
}


CSandbox::CSandbox(const char* sDir, const int lifeTime, const int idLength)
{
	printf("CSandbox::CSandbox called\n");
	init(sDir, lifeTime, idLength);
}


CSandbox::~CSandbox()
{
	printf("CSandbox::~CSandbox called\n");
	//if (this->id) { free(this->id); }
	//if (this->srcDir) { free(this->srcDir); }
	//if (this->sandboxDir) { free(this->sandboxDir); }
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

	if (now >= this->expiry){
		expired = true; 
	}
	return expired;
}


void
CSandbox::renewExpiry(const int grace)
{
	printf("CSandbox::renewExpiry called\n");
	time_t now;
	time(&now);
	this->expiry = now + grace;
}


void
CSandbox::setExpiry(const int exp)
{
	printf("CSandbox::setExpiry called\n");
	this->expiry = exp;
}


string
CSandbox::getSandboxDir(void)
{
	printf("CSandbox::getSandboxDir called\n");
	return this->sandboxDir;
}


string
CSandbox::getDetails()
{
	printf("CSandbox::getDetails called\n");
	std::ostringstream details;
	
	details << "id=" << this->id << endl
		<< "createTime=" << this->createTime << endl
		<< "expiry=" << this->expiry << endl
		<< "srcDir=" << this->srcDir << endl
		<< "sandboxDir=" << this->sandboxDir << endl;
	return details.str();
}

/*****************************************************************************
* PRIVATE MEMBERS FUNCTIONS
*****************************************************************************/

void
CSandbox::init(const char* sDir, const int lifeTime, const int idLength)
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
}


string
CSandbox::createId(const int length)
{
	// Allowed set of chars in sandboxId
	const char charset[] =	"0123456789"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz";
	char* iden = new char[length + 1];
	string idTmp;
	
	printf("CSandbox::createId called\n");
	for (int i = 0; i < length; ++i) {
        	iden[i] = charset[get_random_int() % (sizeof(charset) - 1)];
	}
	iden[length] = '\0';
	idTmp.assign(iden);

	return idTmp;
}
