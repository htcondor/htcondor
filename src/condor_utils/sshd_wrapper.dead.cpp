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

#include "sshd_wrapper.h"
#include "condor_config.h" // for param
#include "directory.h"
#include "my_username.h"
#include "internet.h"
#include "condor_getcwd.h"

SshdWrapper::SshdWrapper()
{
	pubKeyFile = NULL;
	port = 4000; // param for this?
	sinful = NULL;
	dir = NULL;
	username = NULL;
	hostKeyFile = NULL;
}

SshdWrapper::~SshdWrapper()
{
	free(pubKeyFile);
	free(privKeyFile);
	free(hostKeyFile);

		//free(sinful);
		//free(dir);
		//free(username);
	
}

bool
SshdWrapper::initialize(char *filename)
{
	pubKeyFile = filename;

		// ASSERT filename ends in .pub
	privKeyFile = strdup(filename);
	privKeyFile[strlen(privKeyFile) - 4] = '\0';

	hostKeyFile = (char *)malloc(strlen(privKeyFile) + strlen("-host") + 1);
	sprintf(hostKeyFile, "%s-host", privKeyFile);

	return true;
}

bool
SshdWrapper::createIdentityKeys() {
	return this->createIdentityKeys(privKeyFile);
}

bool 
SshdWrapper::createIdentityKeys(char *privateKey)
{
	char *keygen = param("SSH_KEYGEN");

	if( keygen == NULL)	{
		dprintf(D_ALWAYS, "no SSH_KEYGEN in config file, can't create keys\n");
		return false;
	}

	char *args = param("SSH_KEYGEN_ARGS");
	if( args == NULL)	{
		dprintf(D_ALWAYS, "no SSH_KEYGEN_ARGS in config file, can't create keys\n");
		free(keygen);
		return false;
	}

	char *command = (char *)malloc(strlen(keygen) + strlen(args) + strlen(privateKey) + 80);

		// Assume args needs privKeyFile as trailing argument
	sprintf(command, "%s %s %s > /dev/null 2>&1 < /dev/null", keygen, args, privateKey);

	dprintf(D_ALWAYS, "Generating keys with %s\n", command);

		// And run the command...
	int ret = system(command);
	
	free( command );
    free( keygen);
    free( args );

	if (ret == 0) {
		return true;
	} else {
		dprintf(D_ALWAYS, "keygen failed with return code %d\n", ret);
		return false;
	}
} 

char *
SshdWrapper::getPubKeyFromFile()
{
	int ret;

	StatInfo si(pubKeyFile);
		// Get the size of the keyfile
	if (si.Error() != 0) {
		dprintf(D_ALWAYS, "Can't stat filename %s\n", pubKeyFile);
		return 0;
	}

	int length = si.GetFileSize();

	char *buf = (char *) malloc(length + 2);

		// Now read the file in in one swoop
	FILE *f = safe_fopen_wrapper(pubKeyFile, "r");

	if (f == NULL) {
		dprintf(D_ALWAYS, "can't open %s errno is %d\n", pubKeyFile, errno);
		free(buf);
		return 0;
	}

	ret = fread(buf, 1, length, f);
	if (ret < length) {
		dprintf(D_ALWAYS, "can't read from %s\n", pubKeyFile);
		free(buf);
		fclose(f);
		return 0;
	}

	if (f == NULL) {
		dprintf(D_ALWAYS, "Can't open idfilename %s\n", pubKeyFile);
		free(buf);
		fclose(f);
		return 0;
	}

	fclose(f);
		// null terminate
	buf[length + 1] = '0';

	return buf;
}

bool
SshdWrapper::setPubKeyToFile(const char *keystring)
{
	FILE *f = safe_fopen_wrapper(pubKeyFile, "w");
	if (f == NULL) {
		dprintf(D_ALWAYS, "can't open %s errno is %d\n", pubKeyFile, errno);
		return false;
	}

	fwrite(keystring, strlen(keystring), 1, f);
	fclose(f);
	return true;
}

bool
SshdWrapper::getSshdExecInfo(char* & executable, char* & args, char* & env )
{
	executable = param("SSHD");
	if (executable == NULL) {
		dprintf(D_ALWAYS, "Can't find SSHD in config file\n");
		args = NULL;
		env = NULL;
		return false;
	}

	char *rawArgs = param("SSHD_ARGS");
	if (rawArgs == NULL) {
		dprintf(D_ALWAYS, "Can't find SSHD_ARGS in config file\n");
		free(executable);
		executable = NULL;
		args = NULL;
		env = NULL;
		return false;
	}

	createIdentityKeys(hostKeyFile);

	char *buf = (char *) malloc(256 + strlen(pubKeyFile) + strlen(hostKeyFile) + strlen(rawArgs));

	sprintf(buf, "-p%d -oAuthorizedKeysFile=%s -h%s %s", port, pubKeyFile, hostKeyFile, rawArgs);
	args = buf;
	
	free(rawArgs);
	env = "";

	launchTime = (int) time(NULL);

	return true;
}

bool 
SshdWrapper::onSshdExitTryAgain(int exit_status)
{
	int currentTime = (int) time(NULL);

		// If sshd has been running for more than two seconds
		// assume it hasn't failed due to a port collision,
		// so we shouldn't retry
	if( (currentTime - launchTime) > 2) {
		return false;
	}

	port++;
	return true;
}

bool 
SshdWrapper::getSshRuntimeInfo(char* & sinful_string, char* & dir, char* & 
					   username)
{
	char hostname[CONDOR_HOSTNAME_MAX];
	condor_gethostname(hostname, CONDOR_HOSTNAME_MAX);

	sinful_string = (char *) malloc(strlen(hostname) + 10);
	sprintf(sinful_string, "<%s:%d>", hostname, port);

		// Is the dir the cwd?
	MyString wd;
	condor_getcwd(wd);
	dir = strdup(wd.Value());
			
		// At this point am I running as the user?

		// pull the username from the passwd file
	username = my_username();

	return true;
}

bool 
SshdWrapper::setSshRuntimeInfo(const char* sinful_string, const char* dir, 
							   const char* username)
{
	this->sinful = sinful_string;
	this->dir = dir;
	this->username = username;
	return true;
}

bool 
SshdWrapper::sendPrivateKeyAndContactFile(const char* contactFileSrc)
{
	char buf[_POSIX_ARG_MAX];
	
	char *scp = param("SCP");  
	if (scp == NULL) {
		dprintf(D_ALWAYS, "Can't find SCP in config file");
		return false;
	}

	if (sinful == NULL) {
		dprintf(D_ALWAYS, "sendPrivateKeyAndContactFile called before runtime set\n");
		free(scp);
		return false;
	}

	char *dstHost = string_to_ipstr(sinful);
	int port = string_to_port(sinful);

	if (dir == NULL) {
		dprintf(D_ALWAYS, "Destination direction not set, can't send keys\n");
		return false;
	}

	sprintf(buf, "%s -q -B -P %d -i %s -oStrictHostKeyChecking=no -oUserKnownHostsFile=%s %s %s %s:%s > %s 2>&1 < %s", scp, port, privKeyFile, NULL_FILE, privKeyFile, contactFileSrc, dstHost, dir, NULL_FILE, NULL_FILE);

		// and run it...
	dprintf(D_ALWAYS, "Sending keys and contact info via: %s\n", buf);

	int r = system(buf);

 	free(scp);
	if (r == 0) {
		return true;
	} else {
		return false;
	}
}

char* 
SshdWrapper::generateContactFileLine(int node)
{
		// 7 is max string length of a port
	char *buf = (char *) malloc(7 + strlen(sinful) + 1 + strlen(username) + 1 
								+ strlen(dir) + 1);

		// Sinful is in the form <1.2.3.4:xxxx>
	    // remove the <>'s and replace the : with a space
	char *copy = strdup(sinful);
	char *p = copy + 1;

		// Remove trailing '>'
	p[strlen(p) - 1] = '\0';
	while (*p++) {
		if (*p == ':') {
			*p = ' ';
			break;
		}
	}

	sprintf(buf, "%d %s %s %s\n", node, (copy + 1), username, dir);

	free(copy);
	return buf;
}
