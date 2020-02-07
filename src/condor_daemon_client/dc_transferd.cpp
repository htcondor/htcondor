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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "dc_transferd.h"
#include "proc.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "condor_ftp.h"


// // // // //
// DCTransferD
// // // // //


DCTransferD::DCTransferD( const char* the_name, const char* the_pool ) 
	: Daemon( DT_TRANSFERD, the_name, the_pool )
{
}


DCTransferD::~DCTransferD()
{
}


// when a transferd registers itself, it identifies who it is. The connection
// is then held open and the schedd periodically might send more transfer
// requests to the transferd. Also, if the transferd dies, the schedd is 
// informed quickly and reliably due to the closed connection.
bool
DCTransferD::setup_treq_channel(ReliSock **treq_sock_ptr, 
	int timeout, CondorError *errstack) 
{
	ReliSock *rsock;

	if (treq_sock_ptr != NULL) {
		// Our caller wants a pointer to the socket we used to succesfully
		// register the claim. The NULL pointer will represent failure and
		// this will only be set to something real if everything was ok.
		*treq_sock_ptr = NULL;
	}

	
	/////////////////////////////////////////////////////////////////////////
	// Connect to the transfer daemon
	/////////////////////////////////////////////////////////////////////////

	// This call with automatically connect to _addr, which was set in the
	// constructor of this object to be the transferd in question.
	rsock = (ReliSock*)startCommand(TRANSFERD_CONTROL_CHANNEL,
		Stream::reli_sock, timeout, errstack);

	if( ! rsock ) {
		dprintf( D_ALWAYS, "DCTransferD::setup_treq_channel: "
				 "Failed to send command (TRANSFERD_CONTROL_CHANNEL) "
				 "to the schedd\n" );
		errstack->push("DC_TRANSFERD", 1, 
			"Failed to start a TRANSFERD_CONTROL_CHANNEL command.");
		return false;
	}

	/////////////////////////////////////////////////////////////////////////
	// Make sure we are authenticated.
	/////////////////////////////////////////////////////////////////////////

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( rsock, errstack )) {
		dprintf( D_ALWAYS, "DCTransferD::setup_treq_channel() authentication "
				"failure: %s\n", errstack->getFullText().c_str() );
		errstack->push("DC_TRANSFERD", 1, 
			"Failed to authenticate properly.");
		return false;
	}

	rsock->encode();

	/////////////////////////////////////////////////////////////////////////
	// At this point, the socket passed all of the authentication protocols
	// so it is ready for use.
	/////////////////////////////////////////////////////////////////////////

	if (treq_sock_ptr)
		*treq_sock_ptr = rsock;

	return true;
}

// upload the files associated with the jobads to the sandbox at td_sinful
// with the supplied capability.
// The work_ad should contain:
//	ATTR_TREQ_CAPABILITY
//	ATTR_TREQ_FTP
//	ATTR_TREQ_JOBID_ALLOW_LIST
bool 
DCTransferD::upload_job_files(int JobAdsArrayLen, ClassAd* JobAdsArray[], 
		ClassAd *work_ad, CondorError * errstack)
{
	ReliSock *rsock = NULL;
	int timeout = 60 * 60 * 8; // transfers take a long time...
	int i;
	ClassAd reqad, respad;
	std::string cap;
	int ftp;
	int invalid;
	int protocol;
	std::string reason;

	//////////////////////////////////////////////////////////////////////////
	// Connect to the transferd and authenticate
	//////////////////////////////////////////////////////////////////////////

	// This call with automatically connect to _addr, which was set in the
	// constructor of this object to be the transferd in question.
	rsock = (ReliSock*)startCommand(TRANSFERD_WRITE_FILES, Stream::reli_sock,
		timeout, errstack);
	if( ! rsock ) {
		dprintf( D_ALWAYS, "DCTransferD::upload_job_files: "
				 "Failed to send command (TRANSFERD_WRITE_FILES) "
				 "to the schedd\n" );
		errstack->push("DC_TRANSFERD", 1, 
			"Failed to start a TRANSFERD_WRITE_FILES command.");
		return false;
	}

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( rsock, errstack )) {
		dprintf( D_ALWAYS, "DCTransferD::upload_job_files() authentication "
				"failure: %s\n", errstack->getFullText().c_str() );
		errstack->push("DC_TRANSFERD", 1, 
			"Failed to authenticate properly.");
		return false;
	}

	rsock->encode();

	//////////////////////////////////////////////////////////////////////////
	// Query the transferd about the capability/protocol and see if I can 
	// upload my files. It will respond with a classad saying good or bad.
	//////////////////////////////////////////////////////////////////////////

	work_ad->LookupString(ATTR_TREQ_CAPABILITY, cap);
	work_ad->LookupInteger(ATTR_TREQ_FTP, ftp);

	reqad.Assign(ATTR_TREQ_CAPABILITY, cap);
	reqad.Assign(ATTR_TREQ_FTP, ftp);

	// This request ad to the transferd should contain:
	//	ATTR_TREQ_CAPABILITY
	//	ATTR_TREQ_FTP
	putClassAd(rsock, reqad);
	rsock->end_of_message();

	rsock->decode();

	// This response ad to the transferd should contain:
	// ATTR_TREQ_INVALID_REQUEST (set to true)
	// ATTR_TREQ_INVALID_REASON
	//
	// OR
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	getClassAd(rsock, respad);
	rsock->end_of_message();

	respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
	
	if (invalid == TRUE) {
		// The transferd rejected my attempt to upload the fileset
		delete rsock;
		respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
		errstack->push("DC_TRANSFERD", 1, reason.c_str());
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Sort the job ads array into numerically sorted order. The transferd
	// must do the same.
	//////////////////////////////////////////////////////////////////////////

	// TODO

	//////////////////////////////////////////////////////////////////////////
	// Based upon the protocol I've chosen, use that method to upload the
	// files. When using the FileTrans protocol, a child process on the
	// transferd side will be inheriting a socket and accepting the
	// FileTransfer object's protocol.
	//////////////////////////////////////////////////////////////////////////

	// XXX Fix to only send jobads for allowed jobs

	dprintf(D_ALWAYS, "Sending fileset");
	work_ad->LookupInteger(ATTR_TREQ_FTP, protocol);
	switch(protocol) {
		case FTP_CFTP:
			// upload the files using the FileTransfer Object

			for (i=0; i<JobAdsArrayLen; i++) {
				FileTransfer ftrans;
				if ( !ftrans.SimpleInit(JobAdsArray[i], false, false, rsock) )
				{
					delete rsock;
					errstack->push("DC_TRANSFERD", 1, 
						"Failed to initate uploading of files.");
					return false;
				}

				ftrans.setPeerVersion( version() );

				if ( !ftrans.UploadFiles(true,false) ) {
					delete rsock;
					errstack->push("DC_TRANSFERD", 1, 
						"Failed to upload files.");
					return false;
				}

				dprintf(D_ALWAYS | D_NOHEADER, ".");
			}	
			rsock->end_of_message();
			dprintf(D_ALWAYS | D_NOHEADER, "\n");
			break;

		default:
			// Bail due to user error. This client doesn't support the uknown
			// protocol.

			delete rsock;
			errstack->push("DC_TRANSFERD", 1, 
				"Unknown file transfer protocol selected.");
			return false;
			break;
	}

	//////////////////////////////////////////////////////////////////////////
	// Get the response from the transferd once it sees a completed 
	// movement of files from the child process.
	//////////////////////////////////////////////////////////////////////////

	rsock->decode();
	getClassAd(rsock, respad);
	rsock->end_of_message();

	// close up shop
	delete rsock;

	respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
	if ( invalid == TRUE ) {
		respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
		errstack->push("DC_TRANSFERD", 1, reason.c_str());
		return false;
	}

	return true;
}


// download the files associated with the jobads to the sandbox at td_sinful
// with the supplied capability.
// The work_ad should contain:
//	ATTR_TREQ_CAPABILITY
//	ATTR_TREQ_FTP
//	ATTR_TREQ_JOBID_ALLOW_LIST
bool 
DCTransferD::download_job_files(ClassAd *work_ad, CondorError * errstack)
{
	ReliSock *rsock = NULL;
	int timeout = 60 * 60 * 8; // transfers take a long time...
	int i;
	ClassAd reqad, respad;
	std::string cap;
	int ftp;
	int invalid;
	int protocol;
	std::string reason;
	int num_transfers;
	ClassAd jad;
	const char *lhstr = NULL;
	ExprTree *tree = NULL;

	//////////////////////////////////////////////////////////////////////////
	// Connect to the transferd and authenticate
	//////////////////////////////////////////////////////////////////////////

	// This call with automatically connect to _addr, which was set in the
	// constructor of this object to be the transferd in question.
	rsock = (ReliSock*)startCommand(TRANSFERD_READ_FILES, Stream::reli_sock,
		timeout, errstack);
	if( ! rsock ) {
		dprintf( D_ALWAYS, "DCTransferD::download_job_files: "
				 "Failed to send command (TRANSFERD_READ_FILES) "
				 "to the schedd\n" );
		errstack->push("DC_TRANSFERD", 1, 
			"Failed to start a TRANSFERD_READ_FILES command.");
		return false;
	}

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( rsock, errstack )) {
		dprintf( D_ALWAYS, "DCTransferD::download_job_files() authentication "
				"failure: %s\n", errstack->getFullText().c_str() );
		errstack->push("DC_TRANSFERD", 1, 
			"Failed to authenticate properly.");
		return false;
	}

	rsock->encode();

	//////////////////////////////////////////////////////////////////////////
	// Query the transferd about the capability/protocol and see if I can 
	// download my files. It will respond with a classad saying good or bad.
	//////////////////////////////////////////////////////////////////////////

	work_ad->LookupString(ATTR_TREQ_CAPABILITY, cap);
	work_ad->LookupInteger(ATTR_TREQ_FTP, ftp);

	reqad.Assign(ATTR_TREQ_CAPABILITY, cap);
	reqad.Assign(ATTR_TREQ_FTP, ftp);

	// This request ad to the transferd should contain:
	//	ATTR_TREQ_CAPABILITY
	//	ATTR_TREQ_FTP
	putClassAd(rsock, reqad);
	rsock->end_of_message();

	rsock->decode();

	// This response ad from the transferd should contain:
	// ATTR_TREQ_INVALID_REQUEST (set to true)
	// ATTR_TREQ_INVALID_REASON
	//
	// OR
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	//	ATTR_TREQ_NUM_TRANSFERS
	//
	getClassAd(rsock, respad);
	rsock->end_of_message();

	respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
	
	if (invalid == TRUE) {
		// The transferd rejected my attempt to upload the fileset
		delete rsock;
		respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
		errstack->push("DC_TRANSFERD", 1, reason.c_str());
		return false;
	}

	respad.LookupInteger(ATTR_TREQ_NUM_TRANSFERS, num_transfers);

	//////////////////////////////////////////////////////////////////////////
	// Based upon the protocol I've chosen, use that method to download the
	// files. When using the FileTrans protocol, a child process on the
	// transferd side will be sending me individual job ads and then 
	// instantiating a filetransfer object for that ad.
	//////////////////////////////////////////////////////////////////////////

	dprintf(D_ALWAYS, "Receiving fileset");
	work_ad->LookupInteger(ATTR_TREQ_FTP, protocol);
	switch(protocol) {
		case FTP_CFTP: // download the files using the FileTransfer Object
			for (i = 0; i < num_transfers; i++) {

				// Grab a job ad the server is sending us so we know what
				// to receive.
				getClassAd(rsock, jad);
				rsock->end_of_message();

				// translate the job ad by replacing the 
				// saved SUBMIT_ attributes so the download goes into the
				// correct place.
				for( auto itr = jad.begin(); itr != jad.end(); itr++ ) {
					lhstr = itr->first.c_str();
					tree = itr->second;
					if ( lhstr && strncasecmp("SUBMIT_",lhstr,7)==0 ) {
							// this attr name starts with SUBMIT_
							// compute new lhs (strip off the SUBMIT_)
						const char *new_attr_name = strchr(lhstr,'_');
						ExprTree * pTree;
						ASSERT(new_attr_name);
						new_attr_name++;
							// insert attribute
						pTree = tree->Copy();
						jad.Insert(new_attr_name, pTree);
					}
				}	// while next expr
		
				// instantiate a filetransfer object and have it accept the
				// files.
				FileTransfer ftrans;
				if ( !ftrans.SimpleInit(&jad, false, false, rsock) )
				{
					delete rsock;
					errstack->push("DC_TRANSFERD", 1, 
						"Failed to initate uploading of files.");
					return false;
				}

				// We want files to be copied to their final places, so apply
				// any filename remaps when downloading.
				if ( !ftrans.InitDownloadFilenameRemaps(&jad) ) {
					return false;
				}

				ftrans.setPeerVersion( version() );

				if ( !ftrans.DownloadFiles() ) {
					delete rsock;
					errstack->push("DC_TRANSFERD", 1, 
						"Failed to download files.");
					return false;
				}

				dprintf(D_ALWAYS | D_NOHEADER, ".");
			}	
			rsock->end_of_message();

			dprintf(D_ALWAYS | D_NOHEADER, "\n");

			break;

		default:
			// Bail due to user error. This client doesn't support the unknown
			// protocol.

			delete rsock;
			errstack->push("DC_TRANSFERD", 1, 
				"Unknown file transfer protocol selected.");
			return false;
			break;
	}

	//////////////////////////////////////////////////////////////////////////
	// Get the response from the transferd once it sees a completed 
	// movement of files to the child process.
	//////////////////////////////////////////////////////////////////////////

	rsock->decode();
	getClassAd(rsock, respad);
	rsock->end_of_message();

	// close up shop
	delete rsock;

	respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
	if ( invalid == TRUE ) {
		respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
		errstack->push("DC_TRANSFERD", 1, reason.c_str());
		return false;
	}

	return true;
}






















