/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

class SshdWrapper
{
 public:
		/** Create an object to manage our interaction with sshd.
		 */
	SshdWrapper();
	
		/// dtor
	virtual ~SshdWrapper();

		/**
		   @param filename This is the filename to use to
		   store the key files.  It should be an
		   absolute path
		*/
	bool initialize(char *filename);

		/** Create identity keys in filename passed to the
			initialize method (filename.pub, filename.key).
			@return true on success, false on failure.
		*/

	bool createIdentityKeys();

		/** Get the public key as an alphanumeric string.
			@return On failure, returns NULL.  On success, returns
			a pointer to a null-terminated alphanumeric string.
			It is expected this string will be used as input to
			extractPubKeyFromAd(). This pointer must be
			deallocated by the caller with free().
			@see extractPubKeyFromAd
		*/
	char* getPubKeyFromFile();

		/** Set the public key into an sshd happy .pub file.
			@param keyString This should be a null-terminated
			string returned from a previous call to
			getPubKeyFromFile().  @return true on success,
			false on failure.  @see getPubKeyFromFile
		*/
	bool setPubKeyToFile(const char* keyString);



		/** This method returns info on how the sshd should
			be spawned.  @param executable null
			terminated string with the absolute pathname
			to the sshd.  caller is responsible
			deallocating with free().  @param args null
			terminated string with the arguments for the
			sshd; each arg is separated via spaces.
			caller is responsible deallocating with
			free().  @param env null terminated string
			with the environment to use when starting the
			sshd.  each env variable is seperated with
			something, maybe semi-colons.  Who knows? But
			we do know that the caller is responsible
			deallocating with free().  @return On
			failure, returns false and all pointers will
			be NULL.  On success returns true and no
			pointers will be NULL.
		*/
	bool getSshdExecInfo(char* & executable, char* & args, char* & env );

		/**
		   This method decides if we should try to span sshd again.
		   @param exit_status the exit status from running sshd.
		   @return true if we should try again, false if not.
		*/
	bool onSshdExitTryAgain(int exit_status);

		 
		/**
		   Only can call after call to getSshdExecInfo, user
		   responsible for freeing.
		   @param sinful_string address:port
		   @param dir execute directory on the remote side
		   @param username system username to ssh -l 
		   @return true if all went well
		*/
	bool getSshRuntimeInfo(char* & sinful_string, char* & dir, char* & 
						   username);

		/**
		   @param sinful_string address:port
		   @param dir execute directory on the remote side
		   @param username system username to ssh -l 
		   @return true 
		*/

	bool setSshRuntimeInfo(const char* sinful_string, const char* dir, 
						   const char* username);

		/**
		   This method sends (via scp) the private key file and
		   the contact file over.

		   @param contactFileSrc filename of contact file to send
		   @return true if all went well
		*/

	bool sendPrivateKeyAndContactFile(const char* contactFileSrc);

		/**
		   Only can call after setSshRuntimeInfo, NULL
		   @return a user-freeable string containing the line
		   to append to the contact file
		*/

	char* generateContactFileLine(int node);

 protected:

	bool createIdentityKeys(char *privateKey);		 

 private:

	SshdWrapper(const SshdWrapper &) {}
	SshdWrapper &operator=(const SshdWrapper &l) {return *this;}

	char *pubKeyFile;
	char *privKeyFile;
	char *hostKeyFile;

	int port;
	int launchTime;

	const char *sinful;
	const char *dir;
	const char *username;
};
