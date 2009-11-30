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
#include "credd.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "credential.h"
#include "X509credential.h"
#include "X509credentialWrapper.h"
#include "classad/xmlSource.h"
#include "classad/xmlSink.h"
#include "env.h"
#include "internet.h"
#include "condor_config.h"
#include "directory.h"
#include "credd_constants.h"
#include "classadUtil.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "globus_utils.h"
#include "condor_string.h"
#include "condor_mkstemp.h"

#define MAX_CRED_DATA_SIZE 100000
#define DEF_CRED_CHECK_INTERVAL		60	/* seconds */


SimpleList <CredentialWrapper*> credentials;

extern int myproxyGetDelegationReaperId;

StringList super_users;
char * cred_store_dir = NULL;
char * cred_index_file = NULL;
int CheckCredentials_interval;

int default_cred_expire_threshold;


int 
store_cred_handler(Service * service, int i, Stream *stream) {
  void * data = NULL;
  int rtnVal = FALSE;
  int rc;
  char * temp_file_name = NULL;
  bool found_cred;
  CredentialWrapper * temp_cred = NULL;
  int data_size = -1;
  classad::ClassAd * _classad = NULL;
  classad::ClassAd classad;
  std::string classad_cstr;
  char * classad_str = NULL;
  classad::ClassAdParser parser;
  ReliSock * socket = (ReliSock*)stream;
  const char * user = NULL;

  CredentialWrapper * cred_wrapper;

  if (!socket->triedAuthentication()) { 
    CondorError errstack;
    if( ! SecMan::authenticate_sock(socket, WRITE, &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      goto EXIT;
    }
  }

  user = socket->getFullyQualifiedUser();
  dprintf (D_FULLDEBUG, "Request by: %s, %s\n", socket->getOwner(), user);

  socket->decode();

  if (!socket->code (classad_str)) {
    dprintf (D_ALWAYS, "Error receiving credential metadata\n"); 
    goto EXIT;
  }

  classad_cstr = classad_str;
  free (classad_str);

  _classad = parser.ParseClassAd(classad_cstr);
  if (!_classad) {
	  dprintf (D_ALWAYS, "Error: invalid credential metadata %s\n", classad_cstr.c_str());
	  goto EXIT;
  }

  classad = *_classad;
  delete _classad;
  
 
  
  int type;
  if (!classad.EvaluateAttrInt ("Type", type)) {
    dprintf (D_ALWAYS, "Missing Type attribute in classad!\n");
    goto EXIT;
  }


  if (type == X509_CREDENTIAL_TYPE) {
	cred_wrapper = new X509CredentialWrapper (classad);
    dprintf (D_ALWAYS, "Name=%s Size=%d\n", 
			 cred_wrapper->cred->GetName(), 
			 cred_wrapper->cred->GetDataSize());

  } else {
	  dprintf (D_ALWAYS, "Unsupported credential type %d\n", type);
	  goto EXIT;
  }

  cred_wrapper->cred->SetOrigOwner (socket->getOwner()); // original remote uname
  cred_wrapper->cred->SetOwner (user);                   // mapped uname

  // Receive credential data
  data_size = cred_wrapper->cred->GetDataSize();
  if (data_size > MAX_CRED_DATA_SIZE) {
	  dprintf (D_ALWAYS, "ERROR: Credential data size %d > maximum allowed (%d)\n", data_size, MAX_CRED_DATA_SIZE);
	  goto EXIT;
  }

  data = malloc (data_size);
  if (data == NULL) {
	  EXCEPT("Out of memory. Aborting.");
  }
  if (!socket->code_bytes(data,data_size)) {
    dprintf (D_ALWAYS, "Error receiving credential data\n");
    goto EXIT;
  }
  cred_wrapper->cred->SetData (data, data_size);
  

  // Check whether credential under this name already exists
  found_cred=false;
  credentials.Rewind();
  while (credentials.Next(temp_cred)) {
	  if ((strcmp(cred_wrapper->cred->GetName(), 
				  temp_cred->cred->GetName()) == 0) && 
		  (strcmp(cred_wrapper->cred->GetOwner(), 
				  temp_cred->cred->GetOwner()) == 0)) {
		  found_cred=true;
		  break; // found it
	  }
  }

  if (found_cred) {
	  dprintf (D_ALWAYS, "Credential %s for owner %s already exists!\n", 
			   cred_wrapper->cred->GetName(), 
			   cred_wrapper->cred->GetOwner());
	  delete cred_wrapper;
	  socket->encode();
	  int rcred=CREDD_ERROR_CREDENTIAL_ALREADY_EXISTS;
	  socket->code(rcred);
	  goto EXIT;
  }

  
  // Write data to a file
  temp_file_name = dircat (cred_store_dir, "credXXXXXX");
  condor_mkstemp (temp_file_name);
  cred_wrapper->SetStorageName (temp_file_name);
  
  init_user_id_from_FQN (user);
  if (!StoreData(temp_file_name,data,data_size)) {
	delete cred_wrapper;
    socket->encode();
    int rcred = CREDD_UNABLE_TO_STORE;
    socket->code(rcred);
    goto EXIT;
  }

  ((X509CredentialWrapper*)cred_wrapper)->cred->SetRealExpirationTime (
			   x509_proxy_expiration_time(temp_file_name));

  // Write metadata to a file
  credentials.Append (cred_wrapper);
  SaveCredentialList();

  // Write response to the client
  socket->encode();
  rc = CREDD_SUCCESS;
  socket->code(rc);

  dprintf( D_ALWAYS, "Credential name %s owner %s successfully stored\n",
			 cred_wrapper->cred->GetName(), cred_wrapper->cred->GetOwner() );

  if (type == X509_CREDENTIAL_TYPE) {
	((X509Credential*)cred_wrapper->cred)->display( D_FULLDEBUG );
  }
  rtnVal = TRUE;

EXIT:
  if ( data != NULL ) {
	  free (data);
  }
  if ( temp_file_name != NULL ) {
	  delete [] temp_file_name;
  }
  return rtnVal;
}



int 
get_cred_handler(Service * service, int i, Stream *stream) {
  char * name = NULL;
  int rtnVal = FALSE;
  bool found_cred=false;
  CredentialWrapper * cred = NULL;
  char * owner = NULL;
  const char * user = NULL;
  void * data = NULL;

  ReliSock * socket = (ReliSock*)stream;

  // Authenticate
  if (!socket->triedAuthentication()) {
    CondorError errstack;
    if( ! SecMan::authenticate_sock(socket, READ, &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      goto EXIT;
    }
  }

  socket->decode();

  if (!socket->code(name)) {
    dprintf (D_ALWAYS, "Error receiving credential name\n"); 
    goto EXIT;
  }

  user = socket->getFullyQualifiedUser();
  dprintf (D_ALWAYS, "Authenticated as %s\n", user);

  if (strchr (name, ':')) {
    // The name is of the form user:name
    // This better be a super-user!
    // TODO: Check super-user's list

    // Owner is the first part
    owner = strdup (name);
    char * pColon = strchr (owner, ':');
    *pColon = '\0';
    
    // Name is the second part
    sprintf (name, (char*)(pColon+sizeof(char)));
  
    if (strcmp (owner, user) != 0) { 
      dprintf (D_ALWAYS, "Requesting another user's (%s) credential %s\n", owner, name);

      if (!isSuperUser (user)) {
	dprintf (D_ALWAYS, "User %s is NOT super user, request DENIED\n", user);
	goto EXIT;
      } else {
	dprintf (D_FULLDEBUG, "User %s is super user, request GRANTED\n", user);
      }
    }

  } else {
    owner = strdup (user);
  }

  dprintf (D_ALWAYS, "sending cred %s for user %s\n", name, owner);

  credentials.Rewind();
  while (credentials.Next(cred)) {
	  if (cred->cred->GetType() == X509_CREDENTIAL_TYPE) {
		  if ((strcmp(cred->cred->GetName(), name) == 0) && 
			  (strcmp(cred->cred->GetOwner(), owner) == 0)) {
			  found_cred=true;
			  break; // found it
      }
    }
  }
  
  socket->encode();

  if (found_cred) {
    dprintf (D_FULLDEBUG, "Found cred %s\n", cred->GetStorageName());
    
    int data_size;

    
    int rc = LoadData (cred->GetStorageName(), data, data_size);
    dprintf (D_FULLDEBUG, "Credential::LoadData returned %d\n", rc);
    if (rc == 0) {
      goto EXIT;
    }
    
    socket->code (data_size);
    socket->code_bytes (data, data_size);
    dprintf (D_ALWAYS, "Credential name %s for owner %s returned to user %s\n",
			name, owner, user);
  }
  else {
    dprintf (D_ALWAYS, "Cannot find cred %s\n", name);
    int rc = CREDD_CREDENTIAL_NOT_FOUND;
    socket->code (rc);
  }

  rtnVal = TRUE;
EXIT:
  if ( name != NULL) {
	  free (name);
  }
  if ( owner != NULL) {
	  free (owner);
  }
  if ( data != NULL) {
	  free (data);
  }
  return rtnVal;
}


int 
query_cred_handler(Service * service, int i, Stream *stream) {

  classad::ClassAdUnParser unparser;
  std::string adbuffer;

  char * request = NULL;
  int rtnVal = FALSE;
  int length;
  CredentialWrapper * cred = NULL;
  SimpleList <Credential*> result_list;

  ReliSock * socket = (ReliSock*)stream;
  const char * user;


  if (!socket->triedAuthentication()) {
    CondorError errstack;
    if( ! SecMan::authenticate_sock(socket, READ, &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      goto EXIT;
    }
  }
  user = socket->getFullyQualifiedUser();
  dprintf (D_ALWAYS, "Authenticated as %s\n", user);


  socket->decode();
  if (!socket->code(request)) {
    dprintf (D_ALWAYS, "Error receiving request\n"); 
    goto EXIT;
  }

  if ((request != NULL) && (strcmp (request, "*") == 0)) {
      if (!isSuperUser (user)) {
	dprintf (D_ALWAYS, "User %s is NOT super user, request DENIED\n", user);
    goto EXIT;
    }
  }

    
  // Find credentials for this user
  credentials.Rewind();
  while (credentials.Next(cred)) {
	  if (cred->cred->GetType() == X509_CREDENTIAL_TYPE) {
		  if (strcmp(cred->cred->GetOwner(), user) == 0) {
			  result_list.Append (cred->cred);
		  }
	  }
  }



  socket->encode();
  length = result_list.Length();
  dprintf (D_FULLDEBUG, "User has %d credentials\n", length);
  socket->code (length);
	

  Credential * _cred;
  result_list.Rewind();
  while (result_list.Next(_cred)) {
	  classad::ClassAd * _temp = cred->GetMetadata();
	  unparser.Unparse(adbuffer,_temp);
	  char * classad_str = strdup(adbuffer.c_str());
	  socket->code (classad_str);
	  free (classad_str);
	  delete _temp;
  }
  rtnVal = TRUE;
  
EXIT:
  if (request != NULL) {
	  free (request);
  }
  return rtnVal;
}


int 
rm_cred_handler(Service * service, int i, Stream *stream) {
  char * name = NULL;
  int rtnVal = FALSE;
  int rc;
  bool found_cred;
  CredentialWrapper * cred_wrapper = NULL;
  char * owner = NULL;
  const char * user;

  ReliSock * socket = (ReliSock*)stream;

  if (!socket->triedAuthentication()) {
    CondorError errstack;
    if( ! SecMan::authenticate_sock(socket, READ, &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      goto EXIT;
    }
  }


  socket->decode();

  if (!socket->code(name)) {
    dprintf (D_ALWAYS, "Error receiving credential name\n"); 
    goto EXIT;
  }

  user = socket->getFullyQualifiedUser();

  dprintf (D_ALWAYS, "Authenticated as %s\n", user);

  if (strchr (name, ':')) {
    // The name is of the form user:name
    // This better be a super-user!
    // TODO: Check super-user's list

    // Owner is the first part
    owner = strdup (name);
    char * pColon = strchr (owner, ':');
    *pColon = '\0';
    
    // Name is the second part
    sprintf (name, (char*)(pColon+sizeof(char)));
  
    if (strcmp (owner, user) != 0) { 
      dprintf (D_ALWAYS, "Requesting another user's (%s) credential %s\n", owner, name);

      if (!isSuperUser (user)) {
	dprintf (D_ALWAYS, "User %s is NOT super user, request DENIED\n", user);
	goto EXIT;
      } else {
	dprintf (D_FULLDEBUG, "User %s is super user, request GRANTED\n", user);
      }
    }

  } else {
    owner = strdup (user);
  }

  dprintf (D_ALWAYS, "Attempting to delete cred %s for user %s\n", name, owner);
  

  found_cred=false;
  credentials.Rewind();
  while (credentials.Next(cred_wrapper)) {
	  if (cred_wrapper->cred->GetType() == X509_CREDENTIAL_TYPE) {
		  if ((strcmp(cred_wrapper->cred->GetName(), name) == 0) && 
			  (strcmp(cred_wrapper->cred->GetOwner(), owner) == 0)) {
			  credentials.DeleteCurrent();
			  found_cred=true;
			  break; // found it
		  }
	  }
  }


  if (found_cred) {
    priv_state priv = set_root_priv();
    // Remove credential data
    unlink (cred_wrapper->GetStorageName());
    // Save the metadata list
    SaveCredentialList();
    set_priv(priv);
    delete cred_wrapper;
    dprintf (D_ALWAYS, "Removed credential %s for owner %s\n", name, owner);
  } else {
    dprintf (D_ALWAYS, "Unable to remove credential %s:%s (not found)\n", owner, name); 
  }
	    
  
  free (owner);
  
  socket->encode();
 

  rc = (found_cred)?CREDD_SUCCESS:CREDD_CREDENTIAL_NOT_FOUND;
  socket->code(rc);

  rtnVal = TRUE;

EXIT:
  if (name != NULL) {
	  free (name);
  }
  return rtnVal;
}

int
SaveCredentialList() {
  priv_state priv = set_root_priv();
  FILE * fp = safe_fopen_wrapper(cred_index_file, "w");
  if (!fp) {
    set_priv (priv);
    dprintf (D_ALWAYS, "Unable to open credential index file %s!\n", cred_index_file);
    return FALSE;
  }


  classad::ClassAdXMLUnParser unparser;
  CredentialWrapper * pCred = NULL;

  // Clear the old list
  credentials.Rewind();
  while (credentials.Next(pCred)) {
    const classad::ClassAd * pclassad = pCred->cred->GetMetadata();
	classad::ClassAd temp_classad(*pclassad); // lame
    std::string buff;
    unparser.Unparse (buff, &temp_classad);
    fprintf (fp, "%s\n", buff.c_str());
  }

  fclose (fp);
  
  set_priv (priv);
  return TRUE;
}

int
LoadCredentialList () {
  CredentialWrapper * pCred;

  // Clear the old list
  if (!credentials.IsEmpty()) {
    credentials.Rewind();
    while (credentials.Next(pCred)) {
      credentials.DeleteCurrent();
      delete pCred;
    }
  }

  credentials.Rewind();
  
  classad::ClassAdXMLParser parser;
  char buff[50000];
  
  priv_state priv = set_root_priv();

  FILE * fp = safe_fopen_wrapper(cred_index_file, "r");
  
  if (!fp) {
    dprintf (D_FULLDEBUG, "Credential database %s does not exist!\n", cred_index_file);
    set_priv (priv);
    return TRUE;
  }

  while (fgets(buff, 50000, fp)) {
    if ((buff[0] == '\n') || (buff[0] == '\r')) {
      continue;
    }
    
	classad::ClassAd * classad = parser.ParseClassAd (buff);
    int type=0;

    if ((!classad) || (!classad->EvaluateAttrInt ("Type", type))) {
      dprintf (D_ALWAYS, "Invalid classad %s\n", buff);
      set_priv (priv);
      return FALSE;
    }

    if (type == X509_CREDENTIAL_TYPE) {
      pCred = new X509CredentialWrapper (*classad);
      credentials.Append (pCred);
    }
    else {
      dprintf (D_ALWAYS, "Invalid type %d\n",type); 
    }
  }
  fclose (fp);
  set_priv (priv);

  return TRUE;
}

void
CheckCredentials () {
  CredentialWrapper * pCred;
  credentials.Rewind();  
  dprintf (D_FULLDEBUG, "In CheckCredentials()\n");

  // Get current time
  time_t now = time(NULL);

  while (credentials.Next(pCred)) {
    
    init_user_id_from_FQN (pCred->cred->GetOwner());
    priv_state priv = set_user_priv();

    time_t time = pCred->cred->GetRealExpirationTime();
    dprintf (D_FULLDEBUG, "Checking %s:%s = %d\n",
	       pCred->cred->GetOwner(),
               pCred->cred->GetName(),
	       time);

    if (time - now < 0) {
      dprintf (D_FULLDEBUG, "Credential %s:%s expired!\n",
	       pCred->cred->GetOwner(),
	       pCred->cred->GetName());
    }
    else if (time - now < default_cred_expire_threshold) {
      dprintf (D_FULLDEBUG, "Credential %s:%s about to expire\n",
	       pCred->cred->GetOwner(),
	       pCred->cred->GetName());
      if (pCred->cred->GetType() == X509_CREDENTIAL_TYPE) {
	RefreshProxyThruMyProxy ((X509CredentialWrapper*)pCred);
      }
    }
    
    set_priv (priv); // restore old priv
  }
}


int RefreshProxyThruMyProxy(X509CredentialWrapper * proxy)
{
  const char * proxy_filename = proxy->GetStorageName();
  char * myproxy_host = NULL;
  int status;

  if (((X509Credential*)proxy->cred)->GetMyProxyServerHost() == NULL) {
    dprintf (D_ALWAYS, "Skipping %s\n", proxy->cred->GetName());
    return FALSE;
  }

  // First check if a refresh process is already running
  time_t now = time(NULL);

  if (proxy->get_delegation_pid != GET_DELEGATION_PID_NONE) {
    time_t time_started = proxy->get_delegation_proc_start_time;

    // If the old "refresh proxy" proc is still running, kill it
    if (now - time_started > 500) {
      dprintf (D_FULLDEBUG, "MyProxy refresh process pid=%d still running, "
			  "sending signal %d\n",
			   proxy->get_delegation_pid, SIGKILL);
      daemonCore->Send_Signal (proxy->get_delegation_pid, SIGKILL);
	  // Wait for reaper to cleanup.
    } else {
      dprintf (D_FULLDEBUG, "MyProxy refresh process pid=%d still running, "
			  "letting it finish\n",
			   proxy->get_delegation_pid);
	}
	return FALSE;
  }

  proxy->get_delegation_proc_start_time = now;

  // Set up environnment for myproxy-get-delegation
  Env myEnv;
  MyString strBuff;

  if (((X509Credential*)proxy->cred)->GetMyProxyServerDN()) {
    strBuff="MYPROXY_SERVER_DN=";
    strBuff+= ((X509Credential*)proxy->cred)->GetMyProxyServerDN();
    myEnv.SetEnv (strBuff.Value());
    dprintf (D_FULLDEBUG, "%s\n", strBuff.Value());
  }

  strBuff="X509_USER_PROXY=";
  strBuff+=proxy->GetStorageName();
  dprintf (D_FULLDEBUG, "%s\n", strBuff.Value());

  // Get password (this will end up in stdin for myproxy-get-delegation)
  const char * myproxy_password =((X509Credential*)proxy->cred)->GetRefreshPassword();
  if (myproxy_password == NULL ) {
    dprintf (D_ALWAYS, "No MyProxy password specified for %s:%s\n",
	     proxy->cred->GetName(),
	     proxy->cred->GetOwner());
    myproxy_password = "";
  }

  status = pipe (proxy->get_delegation_password_pipe);
  if (status == -1) {
	dprintf (D_ALWAYS, "get_delegation pipe() failed: %s\n", strerror(errno) );
	proxy->get_delegation_reset();
	return FALSE;
  }
  // TODO: check write() return values for errors, short writes.
  write (proxy->get_delegation_password_pipe[1],
	 myproxy_password,
	 strlen (myproxy_password));
  write (proxy->get_delegation_password_pipe[1], "\n", 1);


  // Figure out user name;
  const char * username = proxy->cred->GetOrigOwner();

  // Figure out myproxy host and port
  myproxy_host = getHostFromAddr (((X509Credential*)proxy->cred)->GetMyProxyServerHost());
  int myproxy_port = getPortFromAddr (((X509Credential*)proxy->cred)->GetMyProxyServerHost());

  // construct arguments
  ArgList args;
  args.AppendArg("--verbose ");

  args.AppendArg("--out");
  args.AppendArg(proxy_filename);

  args.AppendArg("--pshost");
  args.AppendArg(myproxy_host);
  if ( myproxy_host != NULL ) {
	  free ( myproxy_host );
  }

  args.AppendArg("--dn_as_username");

  args.AppendArg("--proxy_lifetime");	// hours
  args.AppendArg(6);

  args.AppendArg("--stdin_pass");

  args.AppendArg("--username");
  args.AppendArg(username);

  // Optional port argument
  if (myproxy_port) {
	  args.AppendArg("--psport");
	  args.AppendArg(myproxy_port);
  }

  // Optional credential name
  if	(	((X509Credential*)proxy->cred)->GetCredentialName() && 
  			( ((X509Credential*)proxy->cred)->GetCredentialName() )[0] ) {
	  args.AppendArg("--credname");
	  args.AppendArg(((X509Credential*)proxy->cred)->GetCredentialName());
  }


  // Create temporary file to store myproxy-get-delegation's stderr
  // The file will be owned by the "condor" user

  priv_state priv = set_condor_priv();
  proxy->get_delegation_err_filename = create_temp_file();
  if (proxy->get_delegation_err_filename == NULL) {
	dprintf (D_ALWAYS, "get_delegation create_temp_file() failed: %s\n",
			strerror(errno) );
	proxy->get_delegation_reset();
	return FALSE;
  }
  status = chmod (proxy->get_delegation_err_filename, 0600);
  if (status == -1) {
	dprintf (D_ALWAYS, "chmod() get_delegation_err_filename %s failed: %s\n",
			proxy->get_delegation_err_filename, strerror(errno) );
	proxy->get_delegation_reset();
	return FALSE;
  }


  proxy->get_delegation_err_fd = safe_open_wrapper(proxy->get_delegation_err_filename,O_RDWR);
  if (proxy->get_delegation_err_fd == -1) {
    dprintf (D_ALWAYS, "Error opening get_delegation file %s: %s\n",
	     proxy->get_delegation_err_filename, strerror(errno) );
	proxy->get_delegation_reset();
	return FALSE;
  }
  set_priv (priv);


  int arrIO[3];
  arrIO[0]=proxy->get_delegation_password_pipe[0]; //stdin
  arrIO[1]=-1; //proxy->get_delegation_err_fd;
  arrIO[2]=proxy->get_delegation_err_fd; // stderr


  char * myproxy_get_delegation_pgm = param ("MYPROXY_GET_DELEGATION");
  if (!myproxy_get_delegation_pgm) {
    dprintf (D_ALWAYS, "MYPROXY_GET_DELEGATION not defined in config file\n");
    return FALSE;
  }
  MyString args_string;
  args.GetArgsStringForDisplay(&args_string);
  dprintf (D_ALWAYS, "Calling %s %s\n", myproxy_get_delegation_pgm, args_string.Value());

  int pid = daemonCore->Create_Process (
					myproxy_get_delegation_pgm,		// name
					args,				 			// args
					PRIV_USER_FINAL,				// priv
					myproxyGetDelegationReaperId,	// reaper_id
					FALSE,							// want_command_port
					&myEnv,							// env
					NULL,							// cwd		
					NULL,							// family_info
					NULL,							// sock_inherit_list
					arrIO);							// in/out/err streams
  													// nice_inc
													// job_opt_mask
  free (myproxy_get_delegation_pgm);
  myproxy_get_delegation_pgm = NULL;


  

  if (pid == FALSE) {
    dprintf (D_ALWAYS, "Failed to run myproxy-get-delegation\n");
	proxy->get_delegation_reset();
    return FALSE;
  }

  proxy->get_delegation_pid = pid;

  return TRUE;
}


int MyProxyGetDelegationReaper(Service *, int exitPid, int exitStatus)
{
  dprintf (D_ALWAYS, "MyProxyGetDelegationReaper pid = %d, rc = %d\n", exitPid, exitStatus);

  credentials.Rewind(); 
  CredentialWrapper * cred_wrapper;
  X509CredentialWrapper * matched_entry = NULL;
  while (credentials.Next (cred_wrapper)) {
    if (cred_wrapper->cred->GetType() == X509_CREDENTIAL_TYPE) {
      if (((X509CredentialWrapper*)cred_wrapper)->get_delegation_pid == exitPid) {
	matched_entry = (X509CredentialWrapper*)cred_wrapper;
	break;
      }
    }
  } //elihw

	if (matched_entry) {
		while (exitStatus != 0) {

		int read_fd = matched_entry->get_delegation_err_fd;	// shorthand
		off_t offset = lseek(read_fd, 0, SEEK_SET);	// rewind
		if (offset == (off_t)-1) {
			dprintf (D_ALWAYS, "myproxy-get-delegation for proxy (%s, %s), "
					"stderr tmp file %s lseek() failed: %s\n", 
					matched_entry->cred->GetOwner(),
					matched_entry->cred->GetName(),
					matched_entry->get_delegation_err_filename,
					strerror(errno)
				);
			break;
		}

		struct stat statbuf;
		int status = fstat(read_fd, &statbuf);
		if (status == -1) {
			dprintf (D_ALWAYS, "myproxy-get-delegation for proxy (%s, %s), "
					"stderr tmp file %s fstat() failed: %s\n", 
					matched_entry->cred->GetOwner(),
					matched_entry->cred->GetName(),
					matched_entry->get_delegation_err_filename,
					strerror(errno)
				);
			break;
		}
		matched_entry->get_delegation_err_buff =
		(char *) calloc ( statbuf.st_size + 1, sizeof(char) );
		int bytes_read =
			read (
					read_fd,
					matched_entry->get_delegation_err_buff,
					statbuf.st_size
				);
		if (bytes_read < 0 ) {
			dprintf (D_ALWAYS, "myproxy-get-delegation for proxy (%s, %s), "
					"stderr tmp file %s read() failed: %s\n", 
					matched_entry->cred->GetOwner(),
					matched_entry->cred->GetName(),
					matched_entry->get_delegation_err_filename,
					strerror(errno)
				);
			break;
		}

		if ( WIFEXITED(exitStatus) ) {
			dprintf (D_ALWAYS, "myproxy-get-delegation for proxy (%s, %s),  "
				"exited with status %d, output (top):\n%s\n\n",
				matched_entry->cred->GetOwner(),
				matched_entry->cred->GetName(),
				WEXITSTATUS(exitStatus),
				matched_entry->get_delegation_err_buff
			);
			break;
		} else if ( WIFSIGNALED(exitStatus) ) {
			dprintf (D_ALWAYS, "myproxy-get-delegation for proxy (%s, %s),  "

				"terminated by signal %d, output (top):\n%s\n\n",
				matched_entry->cred->GetOwner(),
				matched_entry->cred->GetName(),
				WTERMSIG(exitStatus),
				matched_entry->get_delegation_err_buff
			);
			break;
		} else {
			dprintf (D_ALWAYS, "myproxy-get-delegation for proxy (%s, %s),  "

				"unknown status %d, output (top):\n%s\n\n",
				matched_entry->cred->GetOwner(),
				matched_entry->cred->GetName(),
				exitStatus,
				matched_entry->get_delegation_err_buff
			);
			break;
		}

    } // if exitStatus != 0

    // Clean up
	matched_entry->get_delegation_reset();
	} else {
		dprintf (D_ALWAYS,
			"Unable to find X509Credential entry for myproxy-get-delegation pid=%d\n", exitPid);
	}

   return TRUE;
}


bool
isSuperUser( const char* user )
{
  if( ! (user)) {
    return false;
  }

  super_users.rewind();
  char * next;
  while ((next = super_users.next())) {
    if (strcmp (user, next ) == 0) {
      return true;
    }
  }

  return false;
}


void
Init() {
  char * tmp = param( "CRED_SUPER_USERS" );
  if( tmp ) {
    super_users.initializeFromString( tmp );
    free( tmp );
  } else {
#if defined(WIN32)
    super_users.initializeFromString("Administrator");
#else
    super_users.initializeFromString("root");
#endif
  }

  char * spool = param ("SPOOL");

  tmp = param ( "CRED_STORE_DIR" );
  if ( tmp ) {
    cred_store_dir = tmp;
  } else {
    cred_store_dir = dircat (spool, "cred");
  } 
  if ( spool != NULL ) {
	  free (spool);
  }

  tmp = param ( "CRED_INDEX_FILE" );
  if (tmp ) {
    cred_index_file = tmp;
  } else {
    cred_index_file = dircat (cred_store_dir, "cred-index");
  }

  // default 1 hr
  default_cred_expire_threshold = param_integer ("DEFAULT_CRED_EXPIRE_THRESHOLD", 3600);

	// default 1 minute
	CheckCredentials_interval =
		param_integer (
			"CRED_CHECK_INTERVAL",		// param name
			DEF_CRED_CHECK_INTERVAL		// default value, seconds
	  );

  struct stat stat_buff;
  if (stat (cred_store_dir, &stat_buff)) {
    dprintf (D_ALWAYS, "ERROR: Cred store directory %s does not exist\n", cred_store_dir);
    DC_Exit (1 );
  }

  if (stat (cred_index_file, &stat_buff)) {
    dprintf (D_ALWAYS, "Creating credential index file %s\n", cred_index_file);
    priv_state priv = set_root_priv();
    int fd = safe_open_wrapper(cred_index_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd != -1) {
      close (fd);
      set_priv (priv);
    } else {
      dprintf (D_ALWAYS, "ERROR: Unable to create credential index file %s\n", cred_index_file);
      set_priv (priv);
      DC_Exit (1 );
    }
  } else {
    if ((stat_buff.st_mode & (S_IRWXG | S_IRWXO)) ||
	(stat_buff.st_uid != getuid())) {
      dprintf (D_ALWAYS, "ERROR: Invalid ownership / permissions on credential index file %s\n", 
	       cred_index_file);
      DC_Exit (1 );
    }
  }

}

int
StoreData (const char * file_name, const void * data, const int data_size) {

  if (!data) {
    return FALSE;
  }


  priv_state priv = set_root_priv();
  dprintf (D_FULLDEBUG, "in StoreData(), euid=%d\n", geteuid());

  int fd = safe_open_wrapper(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0600 );
  if (fd == -1) {
    dprintf (D_ALWAYS, "Unable to store in %s\n", file_name);
    set_priv(priv);
    return FALSE;
  }

  // Change to user owning the cred (assume init_user_ids() has been called)
  fchmod (fd, S_IRUSR | S_IWUSR);
  fchown (fd, get_user_uid(), get_user_gid());

  write (fd, data, data_size);

  close (fd);

  set_priv(priv);
  return TRUE;
}

int
LoadData (const char * file_name, void *& data, int & data_size) {
  priv_state priv = set_root_priv();
  
  int fd = safe_open_wrapper(file_name, O_RDONLY);
  if (fd == -1) {
    fprintf (stderr, "Can't open %s\n", file_name);
    set_priv (priv);
    return FALSE;
  }
  
  char buff [MAX_CRED_DATA_SIZE+1];
  data_size = read (fd, buff, MAX_CRED_DATA_SIZE);
  buff[data_size]='\0';

  close (fd);

  if (data_size <= 0) {
    set_priv (priv);
    return FALSE;
  }

  data = malloc (data_size);

  memcpy (data, buff, data_size);

  set_priv (priv);
  return TRUE;

}


int
init_user_id_from_FQN (const char * _fqn) {

  char * uid = NULL;
  char * domain = NULL;
  char * fqn = NULL;
  
  if (_fqn) {
    fqn = strdup (_fqn);
    uid = fqn;

    // Domain?
    char * pAt = strchr (fqn, '@');
    if (pAt) {
      *pAt='\0';
      domain = pAt+1;
    }
  }
  
  if (uid == NULL) {
    uid = "nobody";
  }

  int rc = init_user_ids (uid, domain);
  dprintf (D_FULLDEBUG, "Switching to user %s@%s, result = %d\n", uid, domain, rc);

  if (fqn)
    free (fqn);

  return rc;
}

