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


#ifndef _CONDOR_EMAIL_H
#define _CONDOR_EMAIL_H

#include <stdio.h>
#include "condor_classad.h"
#include <string>

FILE * email_nonjob_open( const char *email_addr, const char *subject );

FILE * email_admin_open(const char *subject);

void email_close(FILE *mailer);

void email_asciifile_tail( FILE* mailer, const char* filename,
						   int lines );

void email_corefile_tail( FILE* mailer, const char* subsystem_name );

void email_custom_attributes( FILE* mailer, ClassAd* job_ad );

std::string email_check_domain( const char* addr, ClassAd* job_ad );


class Email
{
public:
	Email();
	~Email();

		/** If you want to write your own text, you can open a new
			message and get back the FILE*
		*/
	FILE* open_stream( ClassAd* ad, int exit_reason = -1,
				const char* subject = NULL );

		/** Write exit info about the job into an open Email.
			@param ad Job to extract info from
			@param exit_reason The Condor exit_reason (not status int)
		*/
	bool writeExit( ClassAd* ad, int exit_reason );

		/** This method sucks.  As soon as we have a real solution for
			storing all 4 of these values in the job classad, it
			should be removed.  In the mean time, it's a way to write
			out the network traffic stats for the job into the email.
		*/
	void writeBytes( float run_sent, float run_recv, float tot_sent,
					 float tot_recv );

        /** Write attributes that the user wants into the open Email
            @param ad Job to extract attributes from
        */
    void writeCustom( ClassAd *ad );


		/// Write out the introductory identification for a job
	bool writeJobId( ClassAd* ad );

		/// Send a currently open Email
	bool send();

		/// These methods handle open, write, and send, but offer no
		/// flexibility in the text of the message.
	void sendExit( ClassAd* ad, int exit_reason );
		/** This method sucks.  As soon as we have a real solution for
			storing all 4 of these values in the job classad, it
			should be removed.  In the mean time, it's a way to write
			out the network traffic stats for the job into the email.
		*/
	void sendExitWithBytes( ClassAd* ad, int exit_reason,
							float run_sent, float run_recv,
							float tot_sent, float tot_recv );
		/* TODO
	void sendError( ClassAd* ad, const char* err_summary,
					const char* err_msg );
		*/
	void sendHold( ClassAd* ad, const char* reason );
	void sendRemove( ClassAd* ad, const char* reason );
	void sendRelease( ClassAd* ad, const char* reason );
	void sendHoldAdmin( ClassAd* ad, const char* reason );
	void sendRemoveAdmin( ClassAd* ad, const char* reason );
	void sendReleaseAdmin( ClassAd* ad, const char* reason );

private:
		// // // // // //
		// Data
		// // // // // //

	FILE* fp;	/// The currently open message (if any)
	int cluster;
	int proc;
	bool email_admin;


		// // // // // //
		// Methods
		// // // // // //

		/// Initialize private data
	void init();

		/** Since the email for most of our events should be so
			similar, we put the code in a shared method to avoid
			duplication.
			@param ad ClassAd for the job
			@param reason The reason we're taking the action
			@param action String describing the action we're taking
		*/
	void sendAction( ClassAd* ad, const char* reason,
					 const char* action, int exit_code);

	bool shouldSend( ClassAd* ad, int exit_reason = -1,
					 bool is_error = false );
};

#endif /* _CONDOR_EMAIL_H */
