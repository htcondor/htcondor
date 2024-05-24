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

#ifndef CONDOR_ERROR_CODES_H_INCLUDE
#define CONDOR_ERROR_CODES_H_INCLUDE


const int AUTHENTICATE_ERR_NOT_BUILT = 1001;
const int AUTHENTICATE_ERR_HANDSHAKE_FAILED = 1002;
const int AUTHENTICATE_ERR_OUT_OF_METHODS = 1003;
const int AUTHENTICATE_ERR_METHOD_FAILED = 1004;
const int AUTHENTICATE_ERR_KEYEXCHANGE_FAILED = 1005;
const int AUTHENTICATE_ERR_TIMEOUT = 1006;
const int AUTHENTICATE_ERR_PLUGIN_FAILED = 1007;

const int SECMAN_ERR_INTERNAL = 2001;
const int SECMAN_ERR_INVALID_POLICY = 2002;
const int SECMAN_ERR_CONNECT_FAILED = 2003;
const int SECMAN_ERR_NO_SESSION = 2004;
const int SECMAN_ERR_ATTRIBUTE_MISSING = 2005;
const int SECMAN_ERR_NO_KEY = 2006;
const int SECMAN_ERR_COMMUNICATIONS_ERROR = 2007;
const int SECMAN_ERR_AUTHENTICATION_FAILED = 2008;
const int SECMAN_ERR_CLIENT_AUTH_FAILED = 2009;
const int SECMAN_ERR_AUTHORIZATION_FAILED = 2010;
const int SECMAN_ERR_COMMAND_NOT_REGISTERED = 2011;

const int DAEMON_ERR_INTERNAL = 3001;

const int SCHEDD_ERR_JOB_ACTION_FAILED = 4001;
const int SCHEDD_ERR_SPOOL_FILES_FAILED = 4002;
const int SCHEDD_ERR_UPDATE_GSI_CRED_FAILED = 4003;
const int SCHEDD_ERR_SET_EFFECTIVE_OWNER_FAILED = 4004;
const int SCHEDD_ERR_SET_ATTRIBUTE_FAILED = 4005;
const int SCHEDD_ERR_MISSING_ARGUMENT = 4006;
const int SCHEDD_ERR_EXPORT_FAILED = 4007;

const int GSI_ERR_AQUIRING_SELF_CREDINTIAL_FAILED = 5001;
const int GSI_ERR_REMOTE_SIDE_FAILED = 5002;
const int GSI_ERR_ACQUIRING_SELF_CREDINTIAL_FAILED = 5003;
const int GSI_ERR_AUTHENTICATION_FAILED = 5004;
const int GSI_ERR_COMMUNICATIONS_ERROR = 5005;
const int GSI_ERR_UNAUTHORIZED_SERVER = 5006;
const int GSI_ERR_NO_VALID_PROXY = 5007;
const int GSI_ERR_DNS_CHECK_ERROR = 5008;

const int CEDAR_ERR_CONNECT_FAILED = 6001;
const int CEDAR_ERR_EOM_FAILED = 6002;
const int CEDAR_ERR_PUT_FAILED = 6003;
const int CEDAR_ERR_GET_FAILED = 6004;
const int CEDAR_ERR_REGISTER_SOCK_FAILED = 6005;
const int CEDAR_ERR_STARTCOMMAND_FAILED = 6006;
const int CEDAR_ERR_CANCELED = 6007;
const int CEDAR_ERR_DEADLINE_EXPIRED = 6008;
const int CEDAR_ERR_NO_SHARED_PORT = 6009;

const int FILETRANSFER_INIT_FAILED = 7001;
const int FILETRANSFER_UPLOAD_FAILED = 7002;
const int FILETRANSFER_DOWNLOAD_FAILED = 7003;

const int DAGMAN_ERR_LOG_FILE = 8001;

const int UTIL_ERR_OPEN_FILE = 9001;
const int UTIL_ERR_CLOSE_FILE = 9002;
const int UTIL_ERR_GET_CWD = 9003;
const int UTIL_ERR_LOG_FILE = 9004;

const int DRAINING_ALREADY_IN_PROGRESS    = 10001;
const int DRAINING_NO_MATCHING_REQUEST_ID = 10002;
const int DRAINING_CHECK_EXPR_FAILED      = 10003;

#endif

