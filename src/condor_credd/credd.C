#include "credd.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "credential.h"
#include "X509credential.h"
#include "X509credentialWrapper.h"
#include "condor_xml_classads.h"
#include "env.h"
#include "internet.h"
#include "condor_config.h"
#include "directory.h"

template class SimpleList<Credential*>;

SimpleList <Credential*> credentials;

extern int myproxyGetDelegationReaperId;

StringList super_users;
char * cred_store_dir = NULL;
char * cred_index_file = NULL;

int default_cred_expire_threshold;

int 
store_cred_handler(Service * service, int i, Stream *stream) {

  ReliSock * socket = (ReliSock*)stream;

  if (!socket->isAuthenticated()) { 
    char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
    MyString methods;
    if (p) {
      methods = p;
      free (p);
    } else {
     methods = SecMan::getDefaultAuthenticationMethods();
    }
    CondorError errstack;
    if( ! socket->authenticate(methods.Value(), &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      return FALSE;
    }
  }

  const char * user = socket->getFullyQualifiedUser();
  dprintf (D_FULLDEBUG, "Request by: %s, %s\n", socket->getOwner(), user);

  socket->decode();

  ClassAd classad;
  if (!classad.initFromStream (*socket)) {
    dprintf (D_ALWAYS, "Error receiving credential metadata\n"); 
    return FALSE;
  }
  
  Credential * cred = NULL;
  int type;
  if (!classad.LookupInteger ("Type", type)) {
    dprintf (D_ALWAYS, "Missing Type attribute in classad!\n");
    return FALSE;
  }

  int data_size = -1;
  void * data = NULL;

  if (type == X509_CREDENTIAL_TYPE) {
    cred = new X509CredentialWrapper (classad);
    dprintf (D_ALWAYS, "Name=%s Size=%d\n", cred->GetName(), cred->GetDataSize());
    data_size = cred->GetDataSize();
    data = malloc (data_size);
  } else {
    dprintf (D_ALWAYS, "Wrong credential type %d\n", type);
    return FALSE;
  }

  cred->SetOrigOwner (socket->getOwner()); // original remote uname
  cred->SetOwner (user);                   // mapped uname


  // Receive credential data
  if (!socket->code_bytes(data,data_size)) {
    dprintf (D_ALWAYS, "Error receiving credential data\n");
    return FALSE;
  }

  cred->SetData (data, data_size);

  // Write data to a file
  char * temp_file_name = dircat (cred_store_dir, "credXXXXXX");
  mkstemp (temp_file_name);
  cred->SetStorageName (temp_file_name);
  
  StoreData(temp_file_name,data,data_size);
  
  // Write metadata to a file
  credentials.Append (cred);
  SaveCredentialList();

  // Write response to the client
  socket->encode();
  int rc = 0;
  socket->code(rc);

  return TRUE;
}



int 
get_cred_handler(Service * service, int i, Stream *stream) {
  char name[_POSIX_PATH_MAX];

  ReliSock * socket = (ReliSock*)stream;

  // Authenticate
  if (!socket->isAuthenticated()) {
    char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "READ");
    MyString methods;
    if (p) {
      methods = p;
      free (p);
    } else {
      methods = SecMan::getDefaultAuthenticationMethods();
    }
    CondorError errstack;
    if( ! socket->authenticate(methods.Value(), &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      return FALSE;
    }
  }



  socket->decode();

  char * pstr = (char*)name;
  if (!socket->code(pstr)) {
    dprintf (D_ALWAYS, "Error receiving credential name\n"); 
    return FALSE;
  }

  const char * user = socket->getFullyQualifiedUser();
  char * owner = NULL;

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
	return FALSE;
      } else {
	dprintf (D_FULLDEBUG, "User %s is super user, request GRANTED\n", user);
      }
    }

  } else {
    owner = strdup (user);
  }

  dprintf (D_ALWAYS, "sending cred %s for user %s\n", name, owner);

  Credential * cred = NULL;

  bool found_cred=false;
  credentials.Rewind();
  while (credentials.Next(cred)) {
    if (cred->GetType() == X509_CREDENTIAL_TYPE) {
      if ((strcmp(cred->GetName(), name) == 0) && 
	  (strcmp(cred->GetOwner(), owner) == 0)) {
	found_cred=true;
	break; // found it
      }
    }
  }
  
  free (owner);
  
  socket->encode();

  if (found_cred) {
    dprintf (D_FULLDEBUG, "Found cred %s\n", cred->GetStorageName());
    
    void * data=NULL;
    int data_size;

    
    int rc = LoadData (cred->GetStorageName(), data, data_size);
    dprintf (D_FULLDEBUG, "Credential::LoadData returned %d\n", rc);
    return rc;
    
    socket->code (data_size);
    socket->code_bytes (data, data_size);
    free (data);
  }
  else {
    dprintf (D_ALWAYS, "Cannot find cred %s\n", name);
    int rc = -1;
    socket->code (rc);
  }

  return TRUE;
}


int 
query_cred_handler(Service * service, int i, Stream *stream) {
  char request[_POSIX_PATH_MAX];

  ReliSock * socket = (ReliSock*)stream;

  socket->decode();

  if (!socket->isAuthenticated()) {
    char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "READ");
    MyString methods;
    if (p) {
      methods = p;
      free (p);
    } else {
      methods = SecMan::getDefaultAuthenticationMethods();
    }
    CondorError errstack;
    if( ! socket->authenticate(methods.Value(), &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      return FALSE;
    }
  }


  char * pstr = (char*)request;
  if (!socket->code(pstr)) {
    dprintf (D_ALWAYS, "Error receiving request\n"); 
    return FALSE;
  }


  const char * user = socket->getFullyQualifiedUser();
  char * owner = NULL;

  dprintf (D_ALWAYS, "Authenticated as %s\n", user);

  if (strcmp (request, "*") == 0) {
      if (!isSuperUser (user)) {
	dprintf (D_ALWAYS, "User %s is NOT super user, request DENIED\n", user);
	return FALSE;
    }
  }

    
  Credential * cred = NULL;

  SimpleList <Credential*> result_list;

  bool found_cred=false;
  credentials.Rewind();
  while (credentials.Next(cred)) {
    if (cred->GetType() == X509_CREDENTIAL_TYPE) {
      if (strcmp(cred->GetOwner(), user) == 0) {
	result_list.Append (cred);
      }
    }
  }

  socket->encode();
  int length = result_list.Length();
  socket->code (length);

  result_list.Rewind();
  while (result_list.Next(cred)) {
    ClassAd _temp (*(cred->GetClassAd()));
    _temp.put (*socket);
  }
  
  return TRUE;
}


int 
rm_cred_handler(Service * service, int i, Stream *stream) {
  char name[_POSIX_PATH_MAX];

  ReliSock * socket = (ReliSock*)stream;

  socket->decode();

  char * pstr = (char*)name;
  if (!socket->code(pstr)) {
    dprintf (D_ALWAYS, "Error receiving credential name\n"); 
    return FALSE;
  }

  if (!socket->isAuthenticated()) {
    char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "READ");
    MyString methods;
    if (p) {
      methods = p;
      free (p);
    } else {
      methods = SecMan::getDefaultAuthenticationMethods();
    }
    CondorError errstack;
    if( ! socket->authenticate(methods.Value(), &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      return FALSE;
    }
  }

  const char * user = socket->getFullyQualifiedUser();
  char * owner = NULL;

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
	return FALSE;
      } else {
	dprintf (D_FULLDEBUG, "User %s is super user, request GRANTED\n", user);
      }
    }

  } else {
    owner = strdup (user);
  }

  dprintf (D_ALWAYS, "Attempting to delete cred %s for user %s\n", name, owner);
  
  Credential * cred = NULL;

  bool found_cred=false;
  credentials.Rewind();
  while (credentials.Next(cred)) {
    if (cred->GetType() == X509_CREDENTIAL_TYPE) {
      if ((strcmp(cred->GetName(), name) == 0) && 
	  (strcmp(cred->GetOwner(), owner) == 0)) {
	credentials.DeleteCurrent();
	found_cred=true;
	break; // found it
      }
    }
  }


  if (found_cred) {
    priv_state priv = set_root_priv();
    // Remove credential data
    unlink (cred->GetStorageName());
    // Save the metadata list
    SaveCredentialList();
    set_priv(priv);
    delete cred;
  } else {
    dprintf (D_ALWAYS, "Unable to remove credential %s:%s (not found)\n", owner, name); 
  }
	    
  
  free (owner);
  
  socket->encode();

  int rc = (found_cred)?0:1;
  socket->code(rc);

  return TRUE;
}

int
SaveCredentialList() {
  priv_state priv = set_root_priv();
  FILE * fp = fopen (cred_index_file, "w");
  if (!fp) {
    set_priv (priv);
    dprintf (D_ALWAYS, "Unable to open credential index file %s!\n", cred_index_file);
    return FALSE;
  }

  MyString buff;
  ClassAdXMLUnparser unparser;
  Credential * pCred = NULL;

  // Clear the old list
  credentials.Rewind();
  while (credentials.Next(pCred)) {
    const ClassAd * pclassad = pCred->GetClassAd();
    ClassAd temp_classad(*pclassad); // lame
    unparser.Unparse (&temp_classad, buff);
    fprintf (fp, "%s\n", buff.Value());
  }

  fclose (fp);
  
  set_priv (priv);
  return TRUE;
}

int
LoadCredentialList () {
  Credential * pCred;

  // Clear the old list
  if (!credentials.IsEmpty()) {
    credentials.Rewind();
    while (credentials.Next(pCred)) {
      credentials.DeleteCurrent();
      delete pCred;
    }
  }

  credentials.Rewind();
  
  ClassAdXMLParser parser;
  char buff[10000];
  
  priv_state priv = set_root_priv();

  FILE * fp = fopen (cred_index_file, "r");
  
  if (!fp) {
    dprintf (D_FULLDEBUG, "Credential database %s does not exist!\n", cred_index_file);
    set_priv (priv);
    return TRUE;
  }

  while (fgets(buff, 10000, fp)) {
    if ((buff[0] == '\n') || (buff[0] == '\r')) {
      continue;
    }
    
    printf ("Creating classad from %s\n", buff);
    ClassAd * classad = parser.ParseClassAd (buff);
    int type=0;

    if ((!classad) || (!classad->LookupInteger("Type", type))) {
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

int
CheckCredentials () {
  Credential * pCred;
  credentials.Rewind();  
  dprintf (D_FULLDEBUG, "In CheckCredentials()\n");

  // Get current time
  time_t now = time(NULL);

  while (credentials.Next(pCred)) {
    time_t time = pCred->GetRealExpirationTime();
    dprintf (D_FULLDEBUG, "Checking %s:%s = %d\n",
	       pCred->GetOwner(),
               pCred->GetName(),
	       time);
    
    
    if (time - now < 0) {
      dprintf (D_FULLDEBUG, "Credential %s:%s expired!\n",
	       pCred->GetOwner(),
	       pCred->GetName());
    }
    else if (time - now < default_cred_expire_threshold) {
      dprintf (D_FULLDEBUG, "Credential %s:%s about to expire\n",
	       pCred->GetOwner(),
	       pCred->GetName());
      if (pCred->GetType() == X509_CREDENTIAL_TYPE) {
	RefreshProxyThruMyProxy ((X509CredentialWrapper*)pCred);
      }
    }
    
  }
  
  return TRUE;
}


int RefreshProxyThruMyProxy(X509CredentialWrapper * proxy)
{
  const char * proxy_filename = proxy->GetStorageName();

  if (proxy->GetMyProxyServerHost() == NULL) {
    dprintf (D_ALWAYS, "Skipping %s\n", proxy->GetName());
    return FALSE;
  }

  // First check if a refresh process is already running
  time_t now = time(NULL);

  if (proxy->get_delegation_pid) {
    time_t time_started = proxy->get_delegation_proc_start_time;

    // If the refresh proxy is still running, kill it
    if (now - time_started > 500) {
      dprintf (D_FULLDEBUG, "Refresh process pid=%d still running, sending SIGKILL\n", 
	       proxy->get_delegation_pid);
      daemonCore->Send_Signal (proxy->get_delegation_pid, SIGKILL);
    }
  }

  proxy->get_delegation_pid=0;
  proxy->get_delegation_proc_start_time = now;

  // Set up environnment for myproxy-get-delegation
  Env myEnv;
  char buff[_POSIX_PATH_MAX];

  if (proxy->GetMyProxyServerDN()) {
    sprintf (buff, "MYPROXY_SERVER_DN=%s",
	     proxy->GetMyProxyServerDN());
    myEnv.Put (buff);
    dprintf (D_FULLDEBUG, "%s\n", buff);
  }

  sprintf (buff,"X509_USER_PROXY=%s", proxy->GetStorageName());
  myEnv.Put (buff);
  dprintf (D_FULLDEBUG, "%s\n", buff);


  // Get password (this will end up in stdin for myproxy-get-delegation)
  const char * myproxy_password = proxy->GetRefreshPassword();
  if (myproxy_password == NULL ) {
    dprintf (D_ALWAYS, "No MyProxy password specified for %s:%s\n",
	     proxy->GetName(),
	     proxy->GetOwner());
    myproxy_password = "";
  }

  pipe (proxy->get_delegation_password_pipe);
  write (proxy->get_delegation_password_pipe[1],
	 myproxy_password,
	 strlen (myproxy_password));
  write (proxy->get_delegation_password_pipe[1], "\n", 1);


  // Figure out user name;
  const char * username = proxy->GetOrigOwner();

  // Figure out myproxy host and port
  char * myproxy_host = getHostFromAddr (proxy->GetMyProxyServerHost());
  int myproxy_port = getPortFromAddr (proxy->GetMyProxyServerHost());

  // construt arguments
  char args[1000];
  sprintf(args, "-v -o %s -s %s -d -t %d -S -l %s",
	  proxy_filename,
	  myproxy_host,
	  6,
	  username);


  // Optional port argument
  if (myproxy_port) {
    sprintf (buff, " -p %d ", myproxy_port);
    strcat (args, buff);
  }

  // Optional credential name
  if (proxy->GetCredentialName()) {
    sprintf (buff, " -k %s ", proxy->GetCredentialName());
    strcat (args, buff);
  }
  
  // Create temporary file to store myproxy-get-delegation's stderr
  proxy->get_delegation_err_filename = create_temp_file();
  chmod (proxy->get_delegation_err_filename, 0600);
  proxy->get_delegation_err_fd = open (proxy->get_delegation_err_filename,O_RDWR);
  if (proxy->get_delegation_err_fd == -1) {
    dprintf (D_ALWAYS, "Error opening file %s\n",
	     proxy->get_delegation_err_filename);
  }


  int arrIO[3];
  arrIO[0]=proxy->get_delegation_password_pipe[0]; //stdin
  arrIO[1]=-1; //proxy->get_delegation_err_fd;
  arrIO[2]=proxy->get_delegation_err_fd; // stderr

  char * myproxy_get_delegation_pgm = param ("MYPROXY_GET_DELEGATION");
  if (!myproxy_get_delegation_pgm) {
    dprintf (D_ALWAYS, "MYPROXY_GET_DELEGATION not defined in config file\n");
    return FALSE;
  }


  dprintf (D_ALWAYS, "Calling %s %s\n", myproxy_get_delegation_pgm, args);

  int pid = daemonCore->Create_Process (
					myproxy_get_delegation_pgm,
					args,
					PRIV_USER_FINAL,
					myproxyGetDelegationReaperId,
					FALSE,
					myEnv.getDelimitedString(),
					NULL,	// cwd
					FALSE, // new proc group
					NULL,  // socket inherit
					arrIO); // in/out/err streams

  free (myproxy_get_delegation_pgm);

  

  if (pid == FALSE) {
    dprintf (D_ALWAYS, "Failed to run myproxy-get-delegation\n");

    proxy->get_delegation_err_fd=-1;
    proxy->get_delegation_pid=FALSE;
    close (proxy->get_delegation_err_fd);
    unlink (proxy->get_delegation_err_filename);// Remove the temporary file
    free (proxy->get_delegation_err_filename);
    
    close (proxy->get_delegation_password_pipe[0]);
    close (proxy->get_delegation_password_pipe[1]);
    proxy->get_delegation_password_pipe[0]=-1;
    proxy->get_delegation_password_pipe[1]=-1;
    
    return FALSE;
  }

  proxy->get_delegation_pid = pid;

  return TRUE;
}


int MyProxyGetDelegationReaper(Service *, int exitPid, int exitStatus)
{
  dprintf (D_ALWAYS, "MyProxyGetDelegationReaper pid = %d, rc = %d\n", exitPid, exitStatus);

  credentials.Rewind(); 
  Credential * cred;
  X509CredentialWrapper * matched_entry = NULL;
  while (credentials.Next (cred)) {
    if (cred->GetType() == X509_CREDENTIAL_TYPE) {
      if (((X509CredentialWrapper*)cred)->get_delegation_pid == exitPid) {
	matched_entry = (X509CredentialWrapper*)cred;
	break;
      }
    }
  } //elihw

  if (matched_entry) {
    if (exitStatus != 0) {

      // Display error

      close (matched_entry->get_delegation_err_fd);

      char buff[500];
      buff[0]='\0';
      int fd = open (matched_entry->get_delegation_err_filename, O_RDONLY);
      if (fd != -1) {
	int bytes_read = read (fd, buff, 499);
	close (fd);
	buff[bytes_read]='\0';
      } else {
	dprintf (D_ALWAYS, "WEIRD! Cannot read err file %s\n", matched_entry->get_delegation_err_filename);
      }

      dprintf (D_ALWAYS, "myproxy-get-delegation for proxy (%s, %s),  exited with code %d, output (top):\n%s\n\n",
	       matched_entry->GetOwner(),
	       matched_entry->GetName(),
	       WEXITSTATUS(exitStatus),
	       buff);

    } // if exitStatus != 0

    // Clean up
    close (matched_entry->get_delegation_password_pipe[0]);
    close (matched_entry->get_delegation_password_pipe[1]);
    matched_entry->get_delegation_password_pipe[0]=-1;
    matched_entry->get_delegation_password_pipe[1]=-1;
 
    matched_entry->get_delegation_err_fd=-1;
    matched_entry->get_delegation_pid=0;
    unlink (matched_entry->get_delegation_err_filename);// Remove the temporary file
    free (matched_entry->get_delegation_err_filename);
    matched_entry->get_delegation_err_filename=NULL;
  } else {
    dprintf (D_ALWAYS, "Unable to find X509Credential entry for myproxy-get-delegation pid=%d\n", exitPid);
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

  tmp = param ( "CRED_INDEX_FILE" );
  if (tmp ) {
    cred_index_file = tmp;
  } else {
    cred_index_file = dircat (cred_store_dir, "cred-index");
  }

  // default 1 hr
  default_cred_expire_threshold = param_integer ("DEFAULT_CRED_EXPIRE_THRESHOLD", 3600);

  struct stat stat_buff;
  if (stat (cred_store_dir, &stat_buff)) {
    dprintf (D_ALWAYS, "ERROR: Cred store directory %s does not exist\n", cred_store_dir);
    DC_Exit (1 );
  }

  if (stat (cred_index_file, &stat_buff)) {
    dprintf (D_ALWAYS, "Creating credential index file %s\n", cred_index_file);
    priv_state priv = set_root_priv();
    int fd = open (cred_index_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
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

  int fd = open (file_name, O_WRONLY);
  if (fd == -1) {
    dprintf (D_ALWAYS, "Unable to store in %s\n", file_name);
    set_priv(priv);
    return FALSE;
  }

  write (fd, data, data_size);

  close (fd);

  set_priv(priv);
  return TRUE;
}

int
LoadData (const char * file_name, void *& data, int & data_size) {
  priv_state priv = set_root_priv();
  
  int fd = open (file_name, O_RDONLY);
  if (fd == -1) {
    fprintf (stderr, "Can't open %s\n", file_name);
    set_priv (priv);
    return FALSE;
  }
  char buff [100000];
  
  data_size = read (fd, buff, 100000);
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
