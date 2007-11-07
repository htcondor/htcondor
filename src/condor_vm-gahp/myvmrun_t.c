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

// This program is used to control a VM in VMware.
// This program uses VMware program API library .
//
// This uses the VixJob_Wait function to block after starting each
// asynchronous function. This effectively makes the asynchronous
// functions synchronous, because VixJob_Wait will not return until the
// asynchronous function has completed.
//

#include <stdio.h>
#include <strings.h>
#include "vix.h"

static VixHandle hostHandle = VIX_INVALID_HANDLE;
static VixHandle jobHandle = VIX_INVALID_HANDLE;
static VixError err = VIX_OK;

const char _usage[] = "\n"
"Authentication flags\n"
"-h <hostName>\n"
"-P <hostPort>\n"
"-u <userName>\n"
"-p <password>\n"
"\n"
"COMMAND          PARAMETERS                  DESCRIPTION\n"
"list                                         List all running VMs\n"
"snapshot         Path to vmx file            Create a snapshot of a VM\n"
"deleteSnapshot   Path to vmx file            Remove a snapshot from a VM\n"
"revertToSnapshot Path to vmx file            Set VM state to a snapshot\n";


static void usage(const char* name)
{
	fprintf(stderr, "Usage: %s [Authentication flags] COMMAND [PARAMETERS]\n"
			 "%s\n", name, _usage);
	exit(1);
}

static void connect_local(const char* host, int port, const char* user, const char* passwd)
{
	hostHandle = VIX_INVALID_HANDLE;
	jobHandle = VIX_INVALID_HANDLE;

	// Connect as current user on local host	
	jobHandle = VixHost_Connect(VIX_API_VERSION,
			VIX_SERVICEPROVIDER_VMWARE_SERVER,
			host, // *hostName,
			port, // hostPort,
			user, // *userName,
			passwd, // *password,
			0, // options,
			VIX_INVALID_HANDLE, // propertyListHandle,
			NULL, // *callbackProc,
			NULL); // *clientData);
	sleep(2);
	err = VixJob_Wait(jobHandle, 
			VIX_PROPERTY_JOB_RESULT_HANDLE, 
			&hostHandle,
			VIX_PROPERTY_NONE);
	if (VIX_OK != err) {
		goto abort;
	}
	//Vix_ReleaseHandle(jobHandle);

	return;
abort:
	Vix_ReleaseHandle(jobHandle);
	exit(1);
}

static int url_count = 0;
static char *urls[200];

void VixDiscoveryProc(VixHandle jobHandle, VixEventType eventType,
					  VixHandle moreEventInfo, void *clientData)
{
	char *url = NULL;

	if( VIX_EVENTTYPE_CALLBACK_SIGNALLED == eventType ) {
		int i = 0;
		// searching is done
		printf("Total running VMs: %d\n", url_count);
		for( i = 0 ; i < url_count; i++ ) {
			printf("%s\n", urls[i]);
		}
		return;
	}else if( VIX_EVENTTYPE_FIND_ITEM != eventType) {
		return;
	}

	// Found a virtual machine
	err = Vix_GetProperties(moreEventInfo,
							VIX_PROPERTY_FOUND_ITEM_LOCATION,
							&url,
							VIX_PROPERTY_NONE);

	if( err == VIX_OK ) {
		urls[url_count++] = (char *)strdup(url);
	}

	Vix_FreeBuffer(url);
	Vix_ReleaseHandle(jobHandle);
}
					

static void listvms(void)
{
	jobHandle = VixHost_FindItems(hostHandle,
				VIX_FIND_RUNNING_VMS,
				VIX_INVALID_HANDLE,	// searchCriteria
				-1, // timeout
				VixDiscoveryProc,
				NULL);
	sleep(2);
	VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);

	Vix_ReleaseHandle(jobHandle);
	VixHost_Disconnect(hostHandle);
	return;
}

static void snapshot(const char* file)
{
	VixHandle vmHandle = VIX_INVALID_HANDLE;
	VixHandle snapshotHandle = VIX_INVALID_HANDLE;

	jobHandle = VixVM_Open(hostHandle,
			file,
			NULL, // VixEventProc *callbackProc,
			NULL); // void *clientData);
	sleep(2);
	err = VixJob_Wait(jobHandle, 
			VIX_PROPERTY_JOB_RESULT_HANDLE, 
			&vmHandle,
			VIX_PROPERTY_NONE);
	if (VIX_OK != err) {
		goto abort;
	}
	Vix_ReleaseHandle(jobHandle);

	jobHandle = VixVM_CreateSnapshot(vmHandle,
			NULL,
			NULL,
			0, // options,
			VIX_INVALID_HANDLE,
			NULL, // *callbackProc,
			NULL); // *clientData);
	sleep(2);
	err = VixJob_Wait(jobHandle, 
			VIX_PROPERTY_JOB_RESULT_HANDLE,
			&snapshotHandle,
			VIX_PROPERTY_NONE);
	if (VIX_OK != err) {
		goto abort;
	}

	Vix_ReleaseHandle(jobHandle);
	Vix_ReleaseHandle(snapshotHandle);
	Vix_ReleaseHandle(vmHandle);
	VixHost_Disconnect(hostHandle);
	return;

abort:
	Vix_ReleaseHandle(jobHandle);
	Vix_ReleaseHandle(snapshotHandle);
	Vix_ReleaseHandle(vmHandle);
	VixHost_Disconnect(hostHandle);
	exit(1);
}

static void deleteSnapshot(const char* file)
{
	VixHandle vmHandle = VIX_INVALID_HANDLE;
	VixHandle snapshotHandle = VIX_INVALID_HANDLE;
	int numRootSnapshots;
	int index;

	jobHandle = VixVM_Open(hostHandle,
			file,
			NULL, // VixEventProc *callbackProc,
			NULL); // void *clientData);
	sleep(2);
	err = VixJob_Wait(jobHandle, 
			VIX_PROPERTY_JOB_RESULT_HANDLE, 
			&vmHandle,
			VIX_PROPERTY_NONE);
	if (VIX_OK != err) {
		goto abort;
	}
	Vix_ReleaseHandle(jobHandle);

	// Only 1 snapshot supported in this release
	numRootSnapshots = 1;
	err = VixVM_GetNumRootSnapshots(vmHandle, &numRootSnapshots);
	if( VIX_OK != err ) {
		// Handle the error...
		goto abort;
	}

	for( index=0; index < numRootSnapshots; index++ ) {
		err =  VixVM_GetRootSnapshot(vmHandle, index, &snapshotHandle);
		if( VIX_OK != err ) {
			goto abort;
		}

		jobHandle = VixVM_RemoveSnapshot(vmHandle, snapshotHandle, 
						0, // options
						NULL, // callbackProc
						NULL); // clientData
		sleep(2);
		err = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
		if( VIX_OK != err ) {
			goto abort;
		}
		Vix_ReleaseHandle(jobHandle);

		// Release the snapshot handle because we no longer need it.	
		Vix_ReleaseHandle(snapshotHandle);
		snapshotHandle = VIX_INVALID_HANDLE;
	}

	Vix_ReleaseHandle(jobHandle);
	Vix_ReleaseHandle(snapshotHandle);
	Vix_ReleaseHandle(vmHandle);
	VixHost_Disconnect(hostHandle);
	return;

abort:
	Vix_ReleaseHandle(jobHandle);
	Vix_ReleaseHandle(snapshotHandle);
	Vix_ReleaseHandle(vmHandle);
	VixHost_Disconnect(hostHandle);
	exit(1);
}

static void revertToSnapshot(const char* file)
{
	VixHandle vmHandle = VIX_INVALID_HANDLE;
	VixHandle snapshotHandle = VIX_INVALID_HANDLE;
	int snapshotIndex;

	jobHandle = VixVM_Open(hostHandle,
			file,
			NULL, // VixEventProc *callbackProc,
			NULL); // void *clientData);
	sleep(2);
	err = VixJob_Wait(jobHandle, 
			VIX_PROPERTY_JOB_RESULT_HANDLE, 
			&vmHandle,
			VIX_PROPERTY_NONE);
	if (VIX_OK != err) {
		goto abort;
	}
	Vix_ReleaseHandle(jobHandle);

	// Revert to snapshot #0
	snapshotIndex = 0;
	err = VixVM_GetRootSnapshot(vmHandle, snapshotIndex, &snapshotHandle);
	if( VIX_OK != err ) {
		goto abort;
	}

	jobHandle = VixVM_RevertToSnapshot(vmHandle, snapshotHandle,
					   0, // options
					   VIX_INVALID_HANDLE, // propertyListHandle
					   NULL, // callbackProc
					   NULL); // clientData	

	sleep(2);
	err = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
	if( VIX_OK != err ) {
		goto abort;
	}

	Vix_ReleaseHandle(jobHandle);
	Vix_ReleaseHandle(snapshotHandle);
	Vix_ReleaseHandle(vmHandle);
	VixHost_Disconnect(hostHandle);
	return;

abort:
	Vix_ReleaseHandle(jobHandle);
	Vix_ReleaseHandle(snapshotHandle);
	Vix_ReleaseHandle(vmHandle);
	VixHost_Disconnect(hostHandle);
	exit(1);
}

int main(int argc, char* argv[])
{
	char *prog = argv[0];
	char *host = NULL;
	char *port = NULL;
	char *user = NULL;
	char *passwd = NULL;
	char *command = NULL;
	char *parameter = NULL;
	int i = 1;

	// handle Authentication flags
	while( i < argc ) {
		if( argv[i][0] != '-' ) {
			command = argv[i];
			break;
		}

		if( argc <= i + 1) {
			usage(prog);
		}

		switch( argv[i][1] ) {
		case 'h':
			host = argv[++i];
			break;
		case 'P':
			port = argv[++i];
			break;
		case 'u':
			user = argv[++i];
			break;
		case 'p':
			passwd = argv[++i];
			break;
		default:
			usage(prog);
			break;
		} 
		i++;
	}

	int _port = 0;
	if( !port ) {
		_port = 0;
	}else {
		_port = atoi(port);
	}

	if( !command || command[0] == '\0' ) {
		usage(prog);
	}

	if(!strcasecmp(command, "list")) {
		connect_local(host, _port, user, passwd);
		listvms();
	}else if(!strcasecmp(command, "snapshot")) {
		parameter = argv[++i];
		if( !parameter || parameter[0] == '\0' ) {
			usage(prog);
		}

		connect_local(host, _port, user, passwd);
		snapshot(parameter);
	}else if(!strcasecmp(command, "deleteSnapshot")) {
		parameter = argv[++i];
		if( !parameter || parameter[0] == '\0' ) {
			usage(prog);
		}

		connect_local(host, _port, user, passwd);
		deleteSnapshot(parameter);
	}else if(!strcasecmp(command, "revertToSnapshot")) {
		parameter = argv[++i];
		if( !parameter || parameter[0] == '\0' ) {
			usage(prog);
		}

		connect_local(host, _port, user, passwd);
		revertToSnapshot(parameter);
	}else {
		usage(prog);
	}

	return 0;
}
