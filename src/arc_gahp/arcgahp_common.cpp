/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "simplelist.h"
#include "arcgahp_common.h"
#include "arcCommands.h"

static std::string arc_proxy_host;
static int arc_proxy_port;
static std::string arc_proxy_user;
static std::string arc_proxy_passwd;

// List for all arc commands
static SimpleList<ArcGahpCommand*> arc_gahp_commands;

void set_arc_proxy_server(const char* url)
{
	if( !url ) {
		return;
	}

	// Need to parse host name and port
	if( !strncasecmp("http://", url, strlen("http://"))) {
		arc_proxy_host = url +  strlen("http://");
		arc_proxy_port = 80;
	}else if( !strncasecmp("https://", url, strlen("https://")) ) {
		arc_proxy_host = url +  strlen("https://");
		arc_proxy_port = 443;
	}else {
		arc_proxy_host = url;
		arc_proxy_port = 80;
	}

	/* sateesh added code to even handle proxy username and password */
	/* This code cannot handle passwords containing @ ? */
	/* Exact format of ARC_HTTP_PROXY is -- http://userid:password@host:port */
	size_t pos = arc_proxy_host.find('@');
	if( std::string::npos != pos ) {
		arc_proxy_user = arc_proxy_host.substr(0, pos);

		arc_proxy_host = arc_proxy_host.substr(pos + 1,
		                                       arc_proxy_host.length());

		pos = arc_proxy_user.find(':');
		if( std::string::npos != pos ) {
			arc_proxy_passwd = arc_proxy_user.substr(pos + 1,
			                                         arc_proxy_user.length());
			arc_proxy_user = arc_proxy_user.substr(0, pos);
		}
	}

	pos = arc_proxy_host.find(':');
	if( std::string::npos != pos ) {
		int port =
			atoi(arc_proxy_host.substr(pos + 1,
			                           arc_proxy_host.length()).c_str());

		if( port > 0 ) {
			arc_proxy_port = port;
		}

		arc_proxy_host = arc_proxy_host.substr(0, pos);
	}

	dprintf(D_ALWAYS, "Using proxy server, host=%s, port=%d user=%s\n",
		arc_proxy_host.c_str(), arc_proxy_port,
		arc_proxy_user.c_str());
}

bool get_arc_proxy_server(const char* &host_name, int& port, const char* &user_name, const char* &passwd )
{
	if( arc_proxy_host.empty() == false ) {
		host_name = arc_proxy_host.c_str();
		port = arc_proxy_port;
		user_name = arc_proxy_user.c_str();
		passwd = arc_proxy_passwd.c_str();
		return true;
	}

	return false;
}

GahpRequest::GahpRequest (const char *cmd, const std::string &proxy_file)
{
	m_worker = NULL;
	m_raw_cmd = cmd;
	m_proxy_file = proxy_file;

	if ( parse_gahp_command(cmd, &m_args) )
		m_reqid = (int)strtol(m_args.argv[1], (char **)NULL, 10);
	else
		m_reqid = -1;
}

ArcGahpCommand::ArcGahpCommand(const char* cmd, ioCheckfn iofunc, workerfn workerfunc)
{
	command = cmd;
	iocheckfunction = iofunc;
	workerfunction = workerfunc;
}

void
registerArcGahpCommand(const char* command, ioCheckfn iofunc, workerfn workerfunc)
{
	if( !command ) {
		return;
	}

	ArcGahpCommand* newcommand = new ArcGahpCommand(command, iofunc, workerfunc);
	ASSERT(newcommand);

	arc_gahp_commands.Append(newcommand);
}

int
numofArcCommands(void)
{
	return arc_gahp_commands.Number();
}

int
allArcCommands(StringList &output)
{
	ArcGahpCommand *one_cmd = NULL;

	arc_gahp_commands.Rewind();
	while( arc_gahp_commands.Next(one_cmd) ) {
		output.append(one_cmd->command.c_str());
	}

	return arc_gahp_commands.Number();
}

bool
executeIOCheckFunc(const char* cmd, char **argv, int argc)
{
	if(!cmd) {
		return false;
	}

	ArcGahpCommand *one_cmd = NULL;

	arc_gahp_commands.Rewind();
	while( arc_gahp_commands.Next(one_cmd) ) {
		if( !strcasecmp(one_cmd->command.c_str(), cmd) &&
			one_cmd->iocheckfunction ) {
			return one_cmd->iocheckfunction(argv, argc);
		}
	}

	dprintf (D_ALWAYS, "Unknown command %s\n", cmd);
	return false;
}

bool
executeWorkerFunc(const char* cmd, GahpRequest *gahp_request)
{
	if(!cmd) {
		return false;
	}

	ArcGahpCommand *one_cmd = NULL;

	arc_gahp_commands.Rewind();
	while( arc_gahp_commands.Next(one_cmd) ) {
		if( !strcasecmp(one_cmd->command.c_str(), cmd) &&
			one_cmd->workerfunction ) {
			return one_cmd->workerfunction(gahp_request);
		}
	}
	dprintf (D_ALWAYS, "Unknown command %s\n", cmd);
	return false;
}

int
parse_gahp_command (const char* raw, Gahp_Args* args) {

	if (!raw) {
		dprintf(D_ALWAYS,"ERROR parse_gahp_command: empty command\n");
		return FALSE;
	}

	args->reset();

	int len=strlen(raw);

	char * buff = (char*)malloc(len+1);
	int buff_len = 0;

	for (int i = 0; i<len; i++) {

		if ( raw[i] == '\\' ) {
			i++;			//skip this char
			if (i<(len-1))
				buff[buff_len++] = raw[i];
			continue;
		}

		/* Check if charcater read was whitespace */
		if ( raw[i]==' ' || raw[i]=='\t' || raw[i]=='\r' || raw[i] == '\n') {

			/* Handle Transparency: we would only see these chars
			if they WEREN'T escaped, so treat them as arg separators
			*/
			buff[buff_len++] = '\0';
			args->add_arg( strdup(buff) );
			buff_len = 0;	// re-set temporary buffer

		}
		else {
			// It's just a regular character, save it
			buff[buff_len++] = raw[i];
		}
	}

	/* Copy the last portion */
	buff[buff_len++] = '\0';
	args->add_arg( strdup(buff) );

	free (buff);
	return TRUE;
}

bool
check_read_access_file(const char *file)
{
	if( !file || file[0] == '\0' ) {
		return false;
	}

	int ret = access(file, R_OK);

	if(ret < 0 ) {
		dprintf(D_ALWAYS, "File(%s) can't be read\n", file);
		return false;
	}

	return true;
}

bool
check_create_file(const char *file, mode_t mode)
{
	if( !file || file[0] == '\0' ) {
		return false;
	}

	FILE *fp = NULL;

	fp = safe_fopen_wrapper(file, "w", mode);
	if( !fp ) {
		dprintf(D_ALWAYS, "failed to safe_fopen_wrapper %s in write mode: "
				"safe_fopen_wrapper returns %s\n", file, strerror(errno));
		return false;
	}

	fclose(fp);
	return true;
}

int
verify_number_args (const int is, const int should_be) {
	if (is != should_be) {
		dprintf (D_ALWAYS, "Wrong # of args %d, should be %d\n", is, should_be);
		return FALSE;
	}
	return TRUE;
}

int
verify_min_number_args (const int is, const int minimum) {
	if (is < minimum ) {
		dprintf (D_ALWAYS, "Wrong # of args %d, should be more than or equal to %d\n", is, minimum);
		return FALSE;
	}
	return TRUE;
}

int
verify_request_id (const char * s) {
	unsigned int i;
	for (i=0; i<strlen(s); i++) {
		if (!isdigit(s[i])) {
			dprintf (D_ALWAYS, "Bad request id %s\n", s);
			return FALSE;
		}
	}

	return TRUE;
}

int
verify_string_name(const char * s) {
	if( s == NULL ) {
		dprintf( D_ALWAYS, "verify_string_name() failed: string is NULL.\n" );
		return false;
	}
	if( strlen(s) <= 0 ) {
		dprintf( D_ALWAYS, "verify_string_name() failed: string is zero-length.\n" );
	}
	return true;
}

int
verify_number (const char * s) {
	if (!s || !(*s)) {
		dprintf (D_ALWAYS, "No digit number\n");
		return FALSE;
	}

	int i=0;

	do {
		if (s[i]<'0' || s[i]>'9') {
			dprintf (D_ALWAYS, "Bad digit number %s\n", s);
			return FALSE;
		}
	} while (s[++i]);

	return TRUE;
}

bool check_access_and_secret_key_file(const char* accesskeyfile, const char* secretkeyfile, std::string &err_msg)
{
	// check the accesskeyfile
	if( !check_read_access_file(accesskeyfile) ) {
		formatstr(err_msg, "Cannot_read_access_key_file(%s)", accesskeyfile? accesskeyfile:"");
		dprintf (D_ALWAYS, "Error: %s\n", err_msg.c_str());
		return false;
	}

	// check the accesskeyfile and secretkeyfile
	if( !check_read_access_file(secretkeyfile) ) {
		formatstr(err_msg, "Cannot_read_secret_key_file(%s)", secretkeyfile? secretkeyfile:"");
		dprintf (D_ALWAYS, "Error: %s\n", err_msg.c_str());
		return false;
	}

	return true;
}

// String -> int
int
get_int (const char * blah, int * s) {
	*s = atoi(blah);
	return TRUE;
}

int
get_ulong (const char * blah, unsigned long * s) {
	*s=(unsigned long)atol(blah);
	return TRUE;
}

static void
append_escaped_arg(std::string &buffer, const std::string &arg)
{
	if ( arg.empty() ) {
		buffer += "NULL";
	} else {
		for ( size_t i = 0; i < arg.length(); i++ ) {
			switch ( arg[i] ) {
			case ' ':
			case '\\':
			case '\r':
			case '\n':
				buffer += '\\';
				// Fall through...
				//@fallthrough@
			default:
				buffer += arg[i];
			}
		}
	}
}

std::string
create_result_string(int req_id, const std::string &result_code,
		const std::string &result_msg,
		const std::vector<std::string> &args)
{
	std::string buffer;

	formatstr( buffer, "%d", req_id );

	buffer += ' ';
	append_escaped_arg(buffer, result_code);

	buffer += ' ';
	append_escaped_arg(buffer, result_msg);

	for ( size_t i = 0; i < args.size(); i++ ) {
		buffer += ' ';
		append_escaped_arg(buffer, args[i]);
	}

	buffer += '\n';
	return buffer;
}

std::string
create_output_string (int req_id, const char ** results, const int argc)
{
	std::string buffer;

	formatstr( buffer, "%d", req_id );

	for ( int i = 0; i < argc; i++ ) {
		buffer += ' ';
		if ( results[i] == NULL ) {
			buffer += "NULL";
		} else {
			for ( int j = 0; results[i][j] != '\0'; j++ ) {
				switch ( results[i][j] ) {
				case ' ':
				case '\\':
				case '\r':
				case '\n':
					buffer += '\\';
					// Fall through...
					//@fallthrough@
				default:
					buffer += results[i][j];
				}
			}
		}
	}

	buffer += '\n';
	return buffer;
}

std::string
create_success_result( int req_id, StringList *result_list)
{
	int index_count = 1;
	if( !result_list || (result_list->number() == 0) ) {
		index_count = 1;
	}else {
		index_count = result_list->number();
	}

	const char *tmp_result[index_count + 1];

	tmp_result[0] = ARC_COMMAND_SUCCESS_OUTPUT;

	int i = 1;
	if( result_list && (result_list->number() > 0) ) {
		char *one_result = NULL;
		result_list->rewind();
		while((one_result = result_list->next()) != NULL ) {
			tmp_result[i] = one_result;
			i++;
		}
	}

	return create_output_string (req_id, tmp_result, i);
}

std::string
create_failure_result( int req_id, const char *err_msg, const char* /* err_code */)
{
#if 0
	const char *tmp_result[3];
	tmp_result[0] = ARC_COMMAND_ERROR_OUTPUT;

	if( !err_code ) {
		err_code = GENERAL_GAHP_ERROR_CODE;
	}
	if( !err_msg ) {
		err_msg = GENERAL_GAHP_ERROR_MSG;
	}
	tmp_result[1] = err_code;
	tmp_result[2] = err_msg;

	return create_output_string(req_id, tmp_result, 3);
#else
	const char *tmp_result[1];
	tmp_result[0] = err_msg ? err_msg : GENERAL_GAHP_ERROR_MSG;
	return create_output_string( req_id, tmp_result, 1 );
#endif
}
