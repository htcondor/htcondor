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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_td.h"
#include "list.h"
#include "condor_classad.h"
#include "daemon.h"
#include "dc_schedd.h"

/* This file deals with the processing of various active transfer requests.
	An active transfer request is a request that the transferd must deal with
	on its own, connecting somewhere and either uploading or downloading
	sets of file. */

int
TransferD::process_active_requests_timer()
{
	TransferRequest *treq = NULL;
	TransferRequest *tmp = NULL;
	int found_actives = FALSE;
	std::string key;

	dprintf(D_FULLDEBUG, "Processing active requests\n");

	// iterate over the transfer requests, removing the ones that should be
	// done actively, and then doing each one. For now, this
	// just blasts out all of the work at once by forking a process to
	// do it.

	dprintf(D_ALWAYS, "%d TransferRequest(s) to check for active state\n",
		m_treqs.getNumElements());

	m_treqs.startIterations();
	while(m_treqs.iterate(tmp)) {
		switch(tmp->get_transfer_service()) {
			case TREQ_MODE_ACTIVE:
				/* XXX TODO */
				break;

			case TREQ_MODE_ACTIVE_SHADOW: // XXX DEMO mode
				found_actives = TRUE;
				treq = tmp;
				m_treqs.getCurrentKey(key);
				m_treqs.remove(key);
				process_active_shadow_request(treq);
				// XXX consider the ramifications of this if it failed.
				delete treq;
				treq = NULL;
				break;

			case TREQ_MODE_PASSIVE:
				// do nothing
				break;
			
			default:
				EXCEPT("process_active_requests_timer() programmer error!");
				break;
		}
	}

	return found_actives;
}

// This should probably do whatever it is going to do in the background.
int
TransferD::process_active_request(TransferRequest *treq)
{
	dprintf(D_ALWAYS, "Active treq = %p.\n", treq);

	return FALSE;
}

// XXX demo
// This will start a file transfer to a shadow in the treq ad, then exit
// with a known exit code.
int
TransferD::process_active_shadow_request(TransferRequest *treq)
{
	FileTransfer *ftrans = NULL;
	SimpleList<ClassAd*>* ads;
	ClassAd *job_ad = NULL;

	dprintf(D_ALWAYS, "Doing an active shadow treq.\n");

	ads = treq->todo_tasks();
	dprintf(D_ALWAYS, "%d transfer ad(s) to process\n", ads->Number());
	ads->Rewind();
	ASSERT(ads->Next(job_ad));
	ASSERT(job_ad != NULL);

	ftrans = new FileTransfer(); // when does this get deleted?
	ASSERT( ftrans->Init(job_ad, false, PRIV_USER) == 1);
	ftrans->RegisterCallback(
		(FileTransferHandlerCpp)&TransferD::active_shadow_transfer_completed,this);
	
	ftrans->setPeerVersion( "6.9.0" ); /* XXX hack */

	// XXX DEMO HACK! This information should be in the transfer request
	// packet for this ad instead of on the command line.

	if (g_td.m_features.get_shadow_direction() == "download") {
		dprintf(D_ALWAYS, "Downlading files from shadow...\n");
		if (!ftrans->DownloadFiles(false)) { // not blocking
			EXCEPT("Could not initiate dowload file transfer");
		}
	} else {
		dprintf(D_ALWAYS, "Uploading files from shadow...\n");
		if (!ftrans->UploadFiles(false)) { // not blocking
			EXCEPT("Could not initiate upload file transfer");
		}
	}

	// go back to daemon core.
	return TRUE;
}

// this is a bad hack, after doing a good old style shadow transfer, just exit.
int
TransferD::active_shadow_transfer_completed( FileTransfer *ftrans )
{
	// Make certain the file transfer succeeded.  
	if ( ftrans ) {
		FileTransfer::FileTransferInfo ft_info = ftrans->GetInfo();
		if ( !ft_info.success ) {
			if(!ft_info.try_again) {
				EXCEPT( "active_shadow_transfer_completed: hold not impl.!" );
			}
			EXCEPT( "Failed to transfer files" );
		}
	}

	dprintf(D_ALWAYS, "Exiting after successful old school transfer!\n");
	DC_Exit(0); 

	return TRUE;
}











