/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#include "read_user_log.h"
#include "read_user_log_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "MyString.h"
#include "subsystem_info.h"
#include "simple_arg.h"
#include <stdio.h>

static const char *	VERSION = "0.1.0";

DECL_SUBSYSTEM( "TEST_LOG_READER_STATE", SUBSYSTEM_TYPE_TOOL );

enum Verbosity
{
	VERB_NONE = 0,
	VERB_ERROR,
	VERB_WARNING,
	VERB_ALL
};
enum Command
{
	CMD_NONE,
	CMD_DUMP,
	CMD_LIST,
	CMD_VERIFY
};
enum Which
{
	WHICH_NONE,
	WHICH_SIGNATURE,
	WHICH_VERSION,
	WHICH_UPDATE_TIME,
	WHICH_BASE_PATH,
	WHICH_CUR_PATH,
	WHICH_UNIQ_ID,
	WHICH_SEQUENCE,
	WHICH_MAX_ROTATION,
	WHICH_ROTATION,
	WHICH_OFFSET,
	WHICH_GLOBAL_POSITION,
	WHICH_GLOBAL_RECORD_NUM,
	WHICH_INODE,
	WHICH_CTIME,
	WHICH_SIZE,
	WHICH_ALL,
};
enum DataType
{
	TYPE_STRING,
	TYPE_INT,
	TYPE_FSIZE,
	TYPE_INODE,
	TYPE_TIME,
	TYPE_NONE,
};

struct WhichData
{
	Which		 m_which;
	DataType	 m_type;
	int			 m_max_values;
	char		*m_name;
};
static const WhichData whichList[] =
{
	{ WHICH_SIGNATURE,			TYPE_STRING,	1,  "signature" },
	{ WHICH_VERSION,			TYPE_INT,		2,  "version" },
	{ WHICH_UPDATE_TIME,		TYPE_TIME,		2,  "update-time" },
	{ WHICH_BASE_PATH,			TYPE_STRING,	1,  "base-path" },
	{ WHICH_CUR_PATH,			TYPE_STRING,	1,  "path" },
	{ WHICH_UNIQ_ID,			TYPE_STRING,	1,  "uniq" },
	{ WHICH_SEQUENCE,			TYPE_INT,		2,  "sequence" },
	{ WHICH_MAX_ROTATION,		TYPE_INT,		2,  "max-rotation" },
	{ WHICH_ROTATION,			TYPE_INT,		2,  "rotation" },
	{ WHICH_OFFSET,				TYPE_FSIZE,		2,  "offset" },
	{ WHICH_GLOBAL_POSITION,	TYPE_FSIZE,		2,  "global-position" },
	{ WHICH_GLOBAL_RECORD_NUM,	TYPE_FSIZE,		2,  "global-record" },
	{ WHICH_INODE,				TYPE_INODE,		1,  "inode" },
	{ WHICH_CTIME,				TYPE_TIME,		2,  "ctime" },
	{ WHICH_SIZE,				TYPE_FSIZE,		2,  "size" },
	{ WHICH_ALL,				TYPE_NONE,		0,  "all" },
	{ WHICH_NONE,				TYPE_NONE,		-1, NULL, },
};		  

union IntVal { filesize_t asFsize; int asInt; time_t asTime; };
struct Value
{
	struct { IntVal minVal; IntVal maxVal; } asRange;
	IntVal			 asInt;
	StatStructInode	 asInode;
	const char		*asStr;
};

class Options
{
public:
	Options( void );
	~Options( void ) { };


	int handleOpt( SimpleArg &arg, int & );
	int handleFixed( SimpleArg &arg, int & );

	const char *getFile( void ) const { return m_file; };
	Command getCommand( void ) const { return m_command; };
	const WhichData *getWhich( void ) const { return m_which; };
	Value getValue( void ) const { return m_value; };
	bool isValueRange( void ) const { return m_value_is_range; };
	int getVerbose( void ) const { return m_verbose; };
	bool getNumeric( void ) const { return m_numeric; };
	const char *getUsage( void ) const { return m_usage; };
	void dumpWhichList( void ) const;
	const WhichData *lookupWhich( Which which ) const;
	bool isValueOk( void ) const { return m_num_values >= 1; };

private:
	enum { ST_FILE, ST_CMD, ST_WHICH, ST_VALUES, ST_NONE } m_state;
	const char		*m_file;
	Command			 m_command;
	const WhichData	*m_which;
	Value			 m_value;
	bool			 m_value_is_range;
	int				 m_num_values;
	int				 m_verbose;
	bool			 m_numeric;
	const char		*m_usage;

	bool lookupCommand( const SimpleArg & );
	const WhichData *lookupWhich( const char *arg ) const;
	const WhichData *lookupWhich( const SimpleArg &arg ) const;
	bool parseValue( const SimpleArg &arg );
};

int
CheckArgs(int argc, const char **argv, Options &opts);
int
ReadState(const Options &opts, ReadUserLog::FileState &state );
int
DumpState(const Options &opts, const ReadUserLog::FileState &state,
		  const WhichData *wdata = NULL );
int
VerifyState(const Options &opts, const ReadUserLog::FileState &state );

const char *timestr( struct tm &tm, char *buf = NULL, int bufsize = 0 );
const char *timestr( time_t t, char *buf = NULL, int bufsize = 0 );
const char *chomptime( char *buf );

// Simple term signal handler
static bool	global_done = false;
void handle_sig(int sig)
{
	(void) sig;
	printf( "Got signal; shutting down\n" );
	global_done = true;
}

int
main(int argc, const char **argv)
{
	DebugFlags = D_ALWAYS;

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	dprintf_config("TEST_LOG_READER_STATE");

	Options	opts;
	if ( CheckArgs( argc, argv, opts ) < 0 ) {
		fprintf( stderr, "CheckArgs() failed\n" );
		exit( 1 );
	}
	
	ReadUserLog::FileState	state;
	if ( ReadState( opts, state ) < 0 ) {
		fprintf( stderr, "ReadState() failed\n" );
		exit( 1 );
	}

	int	status = 0;
	switch( opts.getCommand() )
	{
	case CMD_NONE:
		status = -1;
		break;
	case CMD_LIST:
		opts.dumpWhichList( );
		break;
	case CMD_DUMP:
		status = DumpState( opts, state );
		break;
	case CMD_VERIFY:
		status = VerifyState( opts, state );
		break;
	}

	if ( status == 0 ) {
		exit( 0 );
	} else {
		exit( 2 );
	}
}

int
CheckArgs(int argc, const char **argv, Options &opts)
{
	int		status = 0;

	for ( int index = 1;  index < argc && status >= 0;  index++ ) {
		SimpleArg	arg( argv, argc, index );

		if ( arg.Error() ) {
			fprintf( stderr, "Failed parsing '%s'\n", arg.Arg() );
			status = -1;
		}

		if ( status > 0 ) {
			fprintf( stderr,
					 "Warning: ignoring parameter %s\n"
					 "  use -h for help\n",
					 arg.Arg() );
		}
		else if ( arg.ArgIsOpt() ) {
			status = opts.handleOpt( arg, index );
		}
		else {
			status = opts.handleFixed( arg, index );
		}
	}

	if ( NULL == opts.getFile() ) {
		fprintf( stderr,
				 "No state file specified\n"
				 "  use -h for help\n" );
		status = -1;
	}
	else if ( CMD_NONE == opts.getCommand() ) {
		fprintf( stderr,
				 "No command specified\n"
				 "  use -h for help\n" );
		status = -1;
	}
	else if ( CMD_VERIFY == opts.getCommand() ) {
		if ( NULL == opts.getWhich() ||
			 WHICH_NONE == opts.getWhich()->m_which ) {
			fprintf( stderr,
					 "Verify: no field name specified\n"
					 "  use -h for help\n" );
			status = -1;
		}
		else if ( ! opts.isValueOk() ) {
			fprintf( stderr,
					 "Verify: no value (or range) specified\n"
					 "  use -h for help\n" );
			status = -1;
		}

	}
	return status;
}

int
ReadState(const Options &opts, ReadUserLog::FileState &state )
{

	// Create & initialize the state
	ReadUserLog::InitFileState( state );

	int	fd = safe_open_wrapper( opts.getFile(), O_RDONLY, 0 );
	if ( fd < 0 ) {
		fprintf( stderr, "Failed to read state file %s\n", opts.getFile() );
		return -1;
	}

	if ( read( fd, state.buf, state.size ) != state.size ) {
		fprintf( stderr, "Failed reading state file %s\n", opts.getFile() );
		close( fd );
		return -1;
	}
	close( fd );

	return 0;
}


int
DumpState( const Options &opts, 
		   const ReadUserLog::FileState &state,
		   const WhichData *wdata )
{
	ReadUserLogState	rstate( state, 60 );
	const ReadUserLogState::FileState	*istate =
		ReadUserLogState::GetFileStateConst( state );

	if ( NULL == wdata ) {
		wdata = opts.getWhich( );
	}

	switch( wdata->m_which )
	{
	case WHICH_NONE:
		return -1;
		break;

	case WHICH_SIGNATURE:
		printf( "  %s: '%s'\n", wdata->m_name, istate->m_signature );
		break;

	case WHICH_VERSION:
		printf( "  %s: %d\n", wdata->m_name, istate->m_version );
		break;

	case WHICH_UPDATE_TIME:
		if ( opts.getNumeric() ) {
			printf( "  %s: %lu\n", wdata->m_name,
					(long unsigned) istate->m_update_time );
		} else {
			printf( "  %s: '%s'\n", wdata->m_name, 
					timestr(istate->m_update_time) );
		}
		break;

	case WHICH_BASE_PATH:
		printf( "  %s: '%s'\n", wdata->m_name, istate->m_base_path );
		break;

	case WHICH_CUR_PATH:
		printf( "  %s: '%s'\n", wdata->m_name, rstate.CurPath(state) );
		break;

	case WHICH_UNIQ_ID:
		printf( "  %s: '%s'\n", wdata->m_name, istate->m_uniq_id );
		break;

	case WHICH_SEQUENCE:
		printf( "  %s: %d\n", wdata->m_name, istate->m_sequence );
		break;

	case WHICH_MAX_ROTATION:
		printf( "  %s: %d\n", wdata->m_name, istate->m_max_rotations );
		break;

	case WHICH_ROTATION:
		printf( "  %s: %d\n", wdata->m_name, istate->m_rotation );
		break;

	case WHICH_OFFSET:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_offset.asint );
		break;

	case WHICH_GLOBAL_POSITION:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_log_position.asint );
		break;

	case WHICH_GLOBAL_RECORD_NUM:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_log_record.asint );
		break;

	case WHICH_INODE:
		printf( "  %s: %lu\n", wdata->m_name,
				(long unsigned) istate->m_inode );
		break;

	case WHICH_CTIME:
		if ( opts.getNumeric() ) {
			printf( "  %s: %lu\n", wdata->m_name,
					(long unsigned) istate->m_ctime );
		} else {
			printf( "  %s: '%s'\n", wdata->m_name,
					timestr(istate->m_ctime) );
		}
		break;

	case WHICH_SIZE:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_size.asint );
		break;

	case WHICH_ALL:
		DumpState( opts, state, opts.lookupWhich(WHICH_SIGNATURE) );
		DumpState( opts, state, opts.lookupWhich(WHICH_VERSION) );
		DumpState( opts, state, opts.lookupWhich(WHICH_UPDATE_TIME) );
		DumpState( opts, state, opts.lookupWhich(WHICH_BASE_PATH) );
		DumpState( opts, state, opts.lookupWhich(WHICH_CUR_PATH) );
		DumpState( opts, state, opts.lookupWhich(WHICH_UNIQ_ID) );
		DumpState( opts, state, opts.lookupWhich(WHICH_SEQUENCE) );
		DumpState( opts, state, opts.lookupWhich(WHICH_MAX_ROTATION) );
		DumpState( opts, state, opts.lookupWhich(WHICH_ROTATION) );
		DumpState( opts, state, opts.lookupWhich(WHICH_OFFSET) );
		DumpState( opts, state, opts.lookupWhich(WHICH_GLOBAL_POSITION) );
		DumpState( opts, state, opts.lookupWhich(WHICH_GLOBAL_RECORD_NUM) );
		DumpState( opts, state, opts.lookupWhich(WHICH_INODE) );
		DumpState( opts, state, opts.lookupWhich(WHICH_CTIME) );
		DumpState( opts, state, opts.lookupWhich(WHICH_SIZE) );
		break;
		
	default:
		return -1;
	}
	return 0;
}

bool
Compare( const Options &opts, const char *val )
{
	bool	ok;

	ok = ( strcasecmp( val, opts.getValue().asStr ) == 0 );
	if ( !ok && opts.getVerbose() ) {
		printf( "  %s: '%s' != '%s'\n",
				opts.getWhich()->m_name, val, opts.getValue().asStr );
	}
	return ok;
}

bool
Compare( const Options &opts, int val )
{
	bool	ok;

	if ( opts.isValueRange() ) {
		ok = (  ( val >= opts.getValue().asRange.minVal.asInt ) &&
				( val <= opts.getValue().asRange.maxVal.asInt )  );
		
		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: %d not in %d - %d\n",
					opts.getWhich()->m_name,
					val,
					opts.getValue().asRange.minVal.asInt,
					opts.getValue().asRange.maxVal.asInt );
		}
	}
	else {
		ok = ( val == opts.getValue().asInt.asInt );
		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: %d != %d\n",
					opts.getWhich()->m_name,
					val,
					opts.getValue().asInt.asInt );
		}
	}

	return ok;
}

bool
Compare( const Options &opts, filesize_t val )
{
	bool	ok;

	if ( opts.isValueRange() ) {
		ok = (  ( val >= opts.getValue().asRange.minVal.asFsize ) &&
				( val <= opts.getValue().asRange.maxVal.asFsize )  );
		
		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: " FILESIZE_T_FORMAT 
					"not in " FILESIZE_T_FORMAT " - " FILESIZE_T_FORMAT "\n",
					opts.getWhich()->m_name,
					val,
					opts.getValue().asRange.minVal.asFsize,
					opts.getValue().asRange.maxVal.asFsize );
		}
	}
	else {
		ok = ( val == opts.getValue().asInt.asInt );
		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: " FILESIZE_T_FORMAT " != " FILESIZE_T_FORMAT "\n",
					opts.getWhich()->m_name,
					val,
					opts.getValue().asInt.asFsize );
		}
	}

	return ok;
}

bool
Compare( const Options &opts, time_t val )
{
	bool	ok;
	char	b1[64], b2[64], b3[64];

	if ( opts.isValueRange() ) {
		ok = (  ( val >= opts.getValue().asRange.minVal.asTime ) &&
				( val <= opts.getValue().asRange.maxVal.asTime )  );
		
		if ( !ok && opts.getVerbose() ) {
			if ( opts.getNumeric() ) {
				printf( "  %s: %lu not in %lu - %lu\n",
						opts.getWhich()->m_name,
						(unsigned long)val,
						(unsigned long)opts.getValue().asRange.minVal.asTime,
						(unsigned long)opts.getValue().asRange.maxVal.asTime );
			}
			else {
				printf( "  %s: %s not in %s - %s\n",
						opts.getWhich()->m_name,
						timestr( val, b1, sizeof(b1) ),
						timestr( opts.getValue().asRange.minVal.asTime,
								 b2, sizeof(b2) ),
						timestr( opts.getValue().asRange.maxVal.asTime,
								 b3, sizeof(b3) ) );
			}
		}
	}
	else {
		ok = ( val == opts.getValue().asInt.asTime );
		if ( !ok && opts.getVerbose() ) {
			if ( opts.getNumeric() ) {
				printf( "  %s: %lu != %lu\n",
						opts.getWhich()->m_name,
						(unsigned long) val,
						(unsigned long) opts.getValue().asInt.asTime );
			}
			else {
				printf( "  %s: %s != %s\n",
						opts.getWhich()->m_name,
						timestr( val, b1, sizeof(b1) ),
						timestr( opts.getValue().asRange.minVal.asTime,
								 b2, sizeof(b2) ) );
			}
		}
	}

	return ok;
}

bool
Compare( const Options &opts, StatStructInode val )
{
	bool	ok;

	ok = val == opts.getValue().asInode;
	if ( !ok && opts.getVerbose() ) {
			printf( "  %s: %lu != %lu\n",
					opts.getWhich()->m_name,
					(unsigned long) val,
					(unsigned long) opts.getValue().asInode );
	}

	return ok;
}

int
VerifyState(const Options &opts, const ReadUserLog::FileState &state )
{
	ReadUserLogState	rstate( state, 60 );
	const ReadUserLogState::FileState	*istate =
		ReadUserLogState::GetFileStateConst( state );

	const WhichData	*wdata = opts.getWhich( );
	if ( wdata == NULL ) {
		fprintf( stderr, "Verify: no which!\n" );
		return -1;
	}

	bool	ok;
	switch( wdata->m_which )
	{
	case WHICH_SIGNATURE:
		ok = Compare( opts, istate->m_signature );
		break;

	case WHICH_VERSION:
		ok = Compare( opts, istate->m_version );
		break;

	case WHICH_UPDATE_TIME:
		ok = Compare( opts, istate->m_update_time );
		break;

	case WHICH_BASE_PATH:
		ok = Compare( opts, istate->m_base_path );
		break;

	case WHICH_CUR_PATH:
		ok = Compare( opts, rstate.CurPath(state) );
		break;

	case WHICH_UNIQ_ID:
		ok = Compare( opts, istate->m_uniq_id );
		break;

	case WHICH_SEQUENCE:
		ok = Compare( opts, istate->m_sequence );
		break;

	case WHICH_MAX_ROTATION:
		ok = Compare( opts, istate->m_max_rotations );
		break;

	case WHICH_ROTATION:
		ok = Compare( opts, istate->m_rotation );
		break;

	case WHICH_OFFSET:
		ok = Compare( opts, istate->m_offset.asint );
		break;

	case WHICH_GLOBAL_POSITION:
		ok = Compare( opts, istate->m_log_position.asint );
		break;

	case WHICH_GLOBAL_RECORD_NUM:
		ok = Compare( opts, istate->m_log_record.asint );
		break;

	case WHICH_INODE:
		ok = Compare( opts, istate->m_inode );
		break;

	case WHICH_CTIME:
		ok = Compare( opts, istate->m_ctime );
		break;

	case WHICH_SIZE:
		ok = Compare( opts, istate->m_size.asint );
		break;
		
	default:
		return -1;
	}
	return ok ? 0 : 1;
}

const char *
timestr( time_t t, char *buf, int bufsize )
{
	static char	sbuf[64];
	if ( NULL == buf ) {
		buf = sbuf;
		bufsize = sizeof(sbuf);
	}
	
	strncpy( buf, ctime(&t), bufsize );
	return chomptime( buf );
}

const char *
timestr( struct tm &tm, char *buf, int bufsize )
{
	static char	sbuf[64];
	if ( NULL == buf ) {
		buf = sbuf;
		bufsize = sizeof(sbuf);
	}

	strncpy( buf, asctime( &tm ), bufsize );
	buf[bufsize-1] = '\0';
	return chomptime( buf );
}

const char *
chomptime( char *buf )
{
	int len = strlen( buf );
	while ( len && isspace( buf[len-1] ) ) {
		buf[len-1] = '\0';
		len--;
	}
	return buf;
}

Options::Options( void )
{
	m_state = ST_FILE;
	m_file = NULL;
	m_command = CMD_NONE;
	m_which = NULL;
	memset( &m_value, 0, sizeof(m_value) );
	m_value_is_range = false;
	m_num_values = 0;
	m_verbose = VERB_NONE;
	m_numeric = false;
	m_usage = 
		"Usage: test_log_reader_state "
		"[options] <filename> <operation [operands]>\n"
		"  commands: dump verify list\n"
		"    dump/any: dump [what]\n"
		"    verify/numeric: what min max\n"
		"    verify/string:  what [value]\n"
		"    list (list names of things)\n"
		"  --numeric|-n: dump times as numeric\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n"
		"  <filename>: the persistent file to read\n";
}

int
Options::handleOpt( SimpleArg &arg, int &index )
{
	if ( arg.Match('n', "numeric") ) {
		m_numeric = true;

	} else if ( arg.Match('d', "debug") ) {
		if ( arg.hasOpt() ) {
			set_debug_flags( const_cast<char *>(arg.getOpt()) );
			index = arg.ConsumeOpt( );
		} else {
			fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
			printf("%s", m_usage);
			return -1;
		}

	} else if ( arg.Match('v') ) {
		m_verbose++;

	} else if ( arg.Match("verbosity") ) {
		if ( !arg.getOpt( m_verbose ) ) {
			fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
			printf("%s", m_usage);
			return -1;
		}

	} else if ( arg.Match("version") ) {
		printf("test_log_reader_state: %s, %s\n", VERSION, __DATE__);
		return 1;
	}
	else {
		fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
		printf("%s", m_usage);
		return -1;
	}
	return 0;
}	

int
Options::handleFixed( SimpleArg &arg, int & /*index*/ )
{
	if ( ST_FILE == m_state ) {
		m_file = arg.Arg();
		m_state = ST_CMD;
		return 0;
	}
	else if ( ST_CMD == m_state ) {
		if ( !lookupCommand( arg ) ) {
			fprintf(stderr, "Invalid command '%s'\n", arg.Arg() );
			printf("%s", m_usage);
			m_state = ST_NONE;
			return -1;
		}
		if ( CMD_LIST == m_command ) {
			m_state = ST_NONE;
			return 1;
		}
		else if ( CMD_DUMP == m_command ) {
			m_which = lookupWhich( "all" );
			if ( NULL == m_which ) {
				assert( 0 );
			}
		}
		m_state = ST_WHICH;
	}
	else if ( ST_WHICH == m_state ) {
		m_which = lookupWhich( arg );
		if ( NULL == m_which ) {
			fprintf(stderr, "Invalid which '%s'\n", arg.Arg() );
			printf("%s", m_usage);
			m_state = ST_NONE;
			return -1;
		}

		if ( 0 == m_which->m_max_values ) {
			m_state = ST_NONE;
			return 1;
		}
		m_state = ST_VALUES;
	}
	else if (  ( ST_VALUES == m_state ) &&
			   ( m_which ) &&
			   ( m_num_values < m_which->m_max_values ) ) {
		if ( !parseValue( arg ) ) {
			fprintf(stderr, "Invalid value for %s: '%s'\n",
					m_which->m_name, arg.Arg() );
			return -1;
		}
		m_num_values++;
	}
	else {
		fprintf(stderr, "Unexpected arg '%s'\n", arg.Arg() );
		return -1;
	}

	return 0;
}

bool
Options::lookupCommand( const SimpleArg &arg )
{
	const char	*s = arg.Arg( );
	char		c = '\0';

	if ( 1 == strlen(s) ) {
		c = s[0];
	}
	if ( 'd' == c  ||  (0 == strcasecmp( s, "dump" )) ) {
		m_command = CMD_DUMP;
		return true;
	}
	else if ( 'v' == c  ||  (0 == strcasecmp( s, "verify" )) ) {
		m_command = CMD_VERIFY;
		return true;
	}
	else if ( 'l' == c  ||  (0 == strcasecmp( s, "list" )) ) {
		m_command = CMD_LIST;
		return true;
	}
	return false;
}

void
Options::dumpWhichList( void ) const
{
	const WhichData	*which = &whichList[0];
	for( which = &whichList[0]; which->m_which != WHICH_NONE; which++ ) {
		printf( "  %s\n", which->m_name );
	}
}

const WhichData *
Options::lookupWhich( Which arg ) const
{
	const WhichData	*which;
	for( which = &whichList[0]; which->m_which != WHICH_NONE; which++ ) {
		if ( which->m_which == arg ) {
			return which;
		}
	}
	return NULL;
}

const WhichData *
Options::lookupWhich( const char *arg ) const
{
	const WhichData	*which;
	for( which = &whichList[0]; which->m_which != WHICH_NONE; which++ ) {
		if ( 0 == strcasecmp( arg, which->m_name ) ) {
			return which;
		}
	}
	return NULL;
}

const WhichData *
Options::lookupWhich( const SimpleArg &arg ) const
{
	return lookupWhich( arg.Arg() );
}

bool
Options::parseValue( const SimpleArg &arg )
{
	const char *s = arg.Arg( );
	if ( TYPE_STRING == m_which->m_type ) {
		m_value.asStr = s;
		return true;
	}
	if ( ( TYPE_INT == m_which->m_type ) && ( isdigit(*s) ) ) {
		int		i = atoi(s);
		if ( 0 == m_num_values ) {
			m_value.asInt.asInt = i;
			m_value.asRange.minVal.asInt = i;
			m_value.asRange.maxVal.asInt = i;
		}
		else {
			m_value.asRange.maxVal.asInt = i;
			m_value_is_range = true;
		}
		return true;
	}
	if ( ( TYPE_FSIZE == m_which->m_type ) && ( isdigit(*s) ) ) {
		filesize_t		i = (filesize_t) atol(s);
		if ( 0 == m_num_values ) {
			m_value.asInt.asFsize = i;
			m_value.asRange.minVal.asFsize = i;
			m_value.asRange.maxVal.asFsize = i;
		}
		else {
			m_value.asRange.maxVal.asFsize = i;
			m_value_is_range = true;
		}
		return true;
	}
	if ( ( TYPE_TIME == m_which->m_type ) && ( isdigit(*s) ) ) {
		time_t		i = (time_t) atol(s);
		if ( 0 == m_num_values ) {
			m_value.asInt.asTime = i;
			m_value.asRange.minVal.asTime = i;
			m_value.asRange.maxVal.asTime = i;
		}
		else {
			m_value.asRange.maxVal.asTime = i;
			m_value_is_range = true;
		}
		return true;
	}
	if ( ( TYPE_INODE == m_which->m_type ) && ( isdigit(*s) ) ) {
		m_value.asInode = (StatStructInode) atol(s);
		return true;
	}
	return false;
}
