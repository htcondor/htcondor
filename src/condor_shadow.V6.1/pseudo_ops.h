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

#if !defined(_PSEUDO_OPS_H)
#define _PSEUDO_OPS_H

int pseudo_register_job_info( ClassAd* ad );
int pseudo_register_machine_info(char *uiddomain, char *fsdomain, 
								 char *starterAddr, char *full_hostname);
int pseudo_register_starter_info( ClassAd* ad );
int pseudo_get_job_info(ClassAd *&ad, bool &delete_ad);
int pseudo_get_user_info(ClassAd *&ad);
int pseudo_job_exit(int status, int reason, ClassAd* ad);
int pseudo_job_termination( ClassAd *ad );
int pseudo_register_mpi_master_info( ClassAd* ad );
int pseudo_begin_execution( void );
int pseudo_get_file_info_new( const char *path, char *&url );
int pseudo_get_buffer_info( int *bytes_out, int *block_size_out, int *prefetch_bytes_out );
int pseudo_ulog( ClassAd *ad );
int pseudo_get_job_ad( ClassAd* &ad );
int pseudo_get_job_attr( const char *name, std::string &expr );
int pseudo_set_job_attr( const char *name, const char *expr, bool log=false);
int pseudo_constrain( const char *expr );
int pseudo_get_sec_session_info(
	char const *starter_reconnect_session_info,
	std::string &reconnect_session_id,
	std::string &reconnect_session_info,
	std::string &reconnect_session_key,
	char const *starter_filetrans_session_info,
	std::string &filetrans_session_id,
	std::string &filetrans_session_info,
	std::string &filetrans_session_key);
int pseudo_event_notification( const ClassAd & ad );
int pseudo_request_guidance( const ClassAd & request, ClassAd & guidance );

#endif
