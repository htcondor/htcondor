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


#ifndef _CONDOR_DC_STARTER_H
#define _CONDOR_DC_STARTER_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"

class PeekGetFD
{
public:
	virtual ~PeekGetFD() { }
	virtual int getNextFD(const std::string &) = 0;
};

/** The subclass of the Daemon object for talking to a starter
*/
class DCStarter : public Daemon {
public:

		/** Constructor.  The Starter can't be found for the usual
			means, so you don't instantiate the DCStarter with the
			usual stuff (name, pool, etc).  You can't query the
			collector for it, or look for an address file, etc.  So,
			once you instantiate a DCStarter object, you have to call
			initFromClassAd() to initialize the DCStarter.
			@param name The name of the starter, NULL if you don't know
		*/
	DCStarter( const char* name = NULL );

		/// Destructor.
	~DCStarter();

		/** Initialize this object with a ClassAd that contains the
			information we need.  At minimum, it must include
			ATTR_STARTER_IP_ADDR.  Ideally, it'd also have
			ATTR_VERSION, ATTR_NAME, and ATTR_MACHINE.  
			@param ad ClassAd with info about the starter.
			@return true if the ad was valid, false if not
		*/
	bool initFromClassAd( ClassAd* ad );

		/** The starter can't be found through the usual means.
			Instead, when you instantiate a DCStarter object, you give
			it all the info in a ClassAd.  So, locate() just has to
			check that the ClassAd was valid and we have the info we
			need.  
			@return true if we have the info, false if not */
	virtual bool locate( LocateType method=LOCATE_FULL );

		/**
		   @param timeout -1 to leave things alone; 0 for no timeout
		   @param sec_session_id NULL for auto-negotiation
		 */
	bool reconnect( ClassAd* req, ClassAd* reply, ReliSock* rsock, 
					int timeout, char const *sec_session_id);

	// Error - Failed to update, a problem
	// Okay - Success.  Updated
	// Declined - Success.  Other side doesn't want it.  Don't bother
	//      to try again.
	enum X509UpdateStatus { XUS_Error = 0, XUS_Okay = 1, XUS_Declined = 2 };
	X509UpdateStatus updateX509Proxy(const char * filename, char const *sec_session_id);
		// expiration_time: 0 if none; o.w. time of delegated proxy expiration

		// result_expiration_time: set to actual expiration time of
		//                         remote proxy (could be shorter than
		//                         requested if source proxy is not valid
		//                         for long enough)
	X509UpdateStatus delegateX509Proxy(const char * filename, time_t expiration_time,char const *sec_session_id, time_t *result_expiration_time);

	bool createJobOwnerSecSession(int timeout,char const *job_claim_id,char const *starter_sec_session,char const *session_info,std::string &owner_claim_id,std::string &error_msg,std::string &starter_version,std::string &starter_addr);

	bool startSSHD(char const *known_hosts_file,char const *private_client_key_file,char const *preferred_shells,char const *slot_name,char const *ssh_keygen_args,ReliSock &sock,int timeout,char const *sec_session_id,std::string &remote_user,std::string &error_msg,bool &retry_is_sensible);

	bool peek(bool transfer_stdout, ssize_t &stdout_offset, bool transfer_stderr, ssize_t &stderr_offset, const std::vector<std::string> &filenames, std::vector<ssize_t> &offsets, size_t max_bytes, bool &retry_sensible, PeekGetFD &next, std::string &, unsigned, const std::string &, DCTransferQueue *xfer_q);

 private:
	bool is_initialized;

		// I can't be copied (yet)
	DCStarter( const DCStarter& );
	DCStarter& operator = ( const DCStarter& );

};

class StarterHoldJobMsg: public DCMsg {
public:
	StarterHoldJobMsg( char const *hold_reason, int hold_code, int hold_subcode, bool soft );

	bool writeMsg( DCMessenger *messenger, Sock *sock );
	bool readMsg( DCMessenger *messenger, Sock *sock );
	MessageClosureEnum messageSent( DCMessenger *messenger, Sock *sock );

private:
	std::string m_hold_reason;
	int m_hold_code;
	int m_hold_subcode;
	bool m_soft;
};

#endif /* _CONDOR_DC_STARTER_H */
