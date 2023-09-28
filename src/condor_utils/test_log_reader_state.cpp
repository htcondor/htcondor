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
#include "subsystem_info.h"
#include "simple_arg.h"
#include <stdio.h>

static const char *	VERSION = "0.1.0";

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
	CMD_DIFF,
	CMD_LIST,
	CMD_VERIFY,
	CMD_ACCESS
};
enum Field
{
	FIELD_NONE,
	FIELD_SIGNATURE,
	FIELD_VERSION,
	FIELD_UPDATE_TIME,
	FIELD_BASE_PATH,
	FIELD_CUR_PATH,
	FIELD_UNIQ_ID,
	FIELD_SEQUENCE,
	FIELD_MAX_ROTATION,
	FIELD_ROTATION,
	FIELD_OFFSET,
	FIELD_EVENT_NUM,
	FIELD_GLOBAL_POSITION,
	FIELD_GLOBAL_RECORD_NUM,
	FIELD_INODE,
	FIELD_CTIME,
	FIELD_SIZE,
	FIELD_ALL,
};
enum DataType
{
	DTYPE_STRING,
	DTYPE_INT,
	DTYPE_FSIZE,
	DTYPE_INODE,
	DTYPE_TIME,
	DTYPE_NONE,
};

struct FieldData
{
	Field		 m_field;
	DataType	 m_type;
	int			 m_max_values;
	const char	*m_name;
};
static const FieldData fieldList[] =
{
	{ FIELD_SIGNATURE,			DTYPE_STRING,	1,  "signature" },
	{ FIELD_VERSION,			DTYPE_INT,		2,  "version" },
	{ FIELD_UPDATE_TIME,		DTYPE_TIME,		2,  "update-time" },
	{ FIELD_BASE_PATH,			DTYPE_STRING,	1,  "base-path" },
	{ FIELD_CUR_PATH,			DTYPE_STRING,	1,  "path" },
	{ FIELD_UNIQ_ID,			DTYPE_STRING,	1,  "uniq" },
	{ FIELD_SEQUENCE,			DTYPE_INT,		2,  "sequence" },
	{ FIELD_MAX_ROTATION,		DTYPE_INT,		2,  "max-rotation" },
	{ FIELD_ROTATION,			DTYPE_INT,		2,  "rotation" },
	{ FIELD_OFFSET,				DTYPE_FSIZE,	2,  "offset" },
	{ FIELD_EVENT_NUM,			DTYPE_FSIZE,	2,  "event-num" },
	{ FIELD_GLOBAL_POSITION,	DTYPE_FSIZE,	2,  "global-position" },
	{ FIELD_GLOBAL_RECORD_NUM,	DTYPE_FSIZE,	2,  "global-record" },
	{ FIELD_INODE,				DTYPE_INODE,	1,  "inode" },
	{ FIELD_CTIME,				DTYPE_TIME,		2,  "ctime" },
	{ FIELD_SIZE,				DTYPE_FSIZE,	2,  "size" },
	{ FIELD_ALL,				DTYPE_NONE,		0,  "all" },
	{ FIELD_NONE,				DTYPE_NONE,		-1, NULL, },
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
	const char *getFile2( void ) const { return m_file2; };
	Command getCommand( void ) const { return m_command; };
	const FieldData *getField( void ) const { return m_field; };
	Value getValue( void ) const { return m_value; };
	bool isValueRange( void ) const { return m_value_is_range; };
	int getVerbose( void ) const { return m_verbose; };
	bool getNumeric( void ) const { return m_numeric; };
	const char *getUsage( void ) const { return m_usage; };
	void dumpFieldList( void ) const;
	const FieldData *lookupField( Field field ) const;
	bool isValueOk( void ) const { return m_num_values >= 1; };
	bool needStateFile( void ) const { return m_command != CMD_LIST; };
	bool needStateFile2( void ) const { return m_command == CMD_DIFF; };

private:
	enum { ST_CMD,
		   ST_FILE,
		   ST_FIELD, ST_FILE2=ST_FIELD,
		   ST_VALUES,
		   ST_NONE } m_state;
	const char		*m_file;
	const char		*m_file2;
	Command			 m_command;
	const FieldData	*m_field;
	Value			 m_value;
	bool			 m_value_is_range;
	int				 m_num_values;
	int				 m_verbose;
	bool			 m_numeric;
	const char		*m_usage;

	bool lookupCommand( const SimpleArg & );
	const FieldData *lookupField( const char *arg ) const;
	const FieldData *lookupField( const SimpleArg &arg ) const;
	bool parseValue( const SimpleArg &arg );
};

int
CheckArgs(int argc,
		  const char **argv,
		  Options &opts);
int
ReadState(const Options &opts,
		  ReadUserLog::FileState &state );
int
DumpState(const Options &opts,
		  const ReadUserLog::FileState &state,
		  const FieldData *wdata = NULL );
int
DiffState(const Options &opts,
		  const ReadUserLog::FileState &state,
		  const ReadUserLog::FileState &state2 );
int
VerifyState(const Options &opts,
			const ReadUserLog::FileState &state );
int
CheckStateAccess(const Options &opts,
				 const ReadUserLog::FileState &state );

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
	set_debug_flags(NULL, D_ALWAYS);

	set_mySubSystem( "TEST_LOG_READER_STATE", false, SUBSYSTEM_TYPE_TOOL );

		// initialize to read from config file
	config();

		// Set up the dprintf stuff...
	dprintf_config("TEST_LOG_READER_STATE");

	Options	opts;
	if ( CheckArgs( argc, argv, opts ) < 0 ) {
		fprintf( stderr, "CheckArgs() failed\n" );
		exit( 1 );
	}

	ReadUserLog::FileState	state;
	if ( opts.needStateFile() && ( ReadState( opts, state ) < 0 )  ) {
		fprintf( stderr, "ReadState() failed\n" );
		exit( 1 );
	}
	ReadUserLog::FileState	state2;
	if ( opts.needStateFile2() && ( ReadState( opts, state2 ) < 0 )  ) {
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
		opts.dumpFieldList( );
		break;
	case CMD_DUMP:
		status = DumpState( opts, state );
		break;
	case CMD_DIFF:
		status = DiffState( opts, state, state2 );
		break;
	case CMD_ACCESS:
		status = CheckStateAccess( opts, state );
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

	for ( int argno = 1;  argno < argc && status >= 0;  argno++ ) {
		SimpleArg	arg( argv, argc, argno );

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
			status = opts.handleOpt( arg, argno );
		}
		else {
			status = opts.handleFixed( arg, argno );
		}
	}

	if ( ( opts.needStateFile() ) && ( NULL == opts.getFile() ) ) {
		fprintf( stderr,
				 "No state file specified\n"
				 "  use -h for help\n" );
		status = -1;
	}
	else if ( ( opts.needStateFile2() ) && ( NULL == opts.getFile2() ) ) {
		fprintf( stderr,
				 "2nd State file not specified\n"
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
		if ( NULL == opts.getField() ||
			 FIELD_NONE == opts.getField()->m_field ) {
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
ReadState(const Options				&opts,
		  ReadUserLog::FileState	&state )
{

	// Create & initialize the state
	ReadUserLog::InitFileState( state );

	printf( "Reading state %s\n", opts.getFile() );
	int	fd = safe_open_wrapper_follow( opts.getFile(), O_RDONLY, 0 );
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
		   const FieldData *wdata )
{
	ReadUserLogState	rstate( state, 60 );
	const ReadUserLogState::FileState	*istate;
	ReadUserLogState::convertState( state, istate );

	if ( NULL == wdata ) {
		wdata = opts.getField( );
	}

	switch( wdata->m_field )
	{
	case FIELD_NONE:
		return -1;
		break;

	case FIELD_SIGNATURE:
		printf( "  %s: '%s'\n", wdata->m_name, istate->m_signature );
		break;

	case FIELD_VERSION:
		printf( "  %s: %d\n", wdata->m_name, istate->m_version );
		break;

	case FIELD_UPDATE_TIME:
		if ( opts.getNumeric() ) {
			printf( "  %s: %lu\n", wdata->m_name,
					(long unsigned) istate->m_update_time );
		} else {
			printf( "  %s: '%s'\n", wdata->m_name,
					timestr(istate->m_update_time) );
		}
		break;

	case FIELD_BASE_PATH:
		printf( "  %s: '%s'\n", wdata->m_name, istate->m_base_path );
		break;

	case FIELD_CUR_PATH:
		printf( "  %s: '%s'\n", wdata->m_name, rstate.CurPath(state) );
		break;

	case FIELD_UNIQ_ID:
		printf( "  %s: '%s'\n", wdata->m_name, istate->m_uniq_id );
		break;

	case FIELD_SEQUENCE:
		printf( "  %s: %d\n", wdata->m_name, istate->m_sequence );
		break;

	case FIELD_MAX_ROTATION:
		printf( "  %s: %d\n", wdata->m_name, istate->m_max_rotations );
		break;

	case FIELD_ROTATION:
		printf( "  %s: %d\n", wdata->m_name, istate->m_rotation );
		break;

	case FIELD_OFFSET:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_offset.asint );
		break;

	case FIELD_EVENT_NUM:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_event_num.asint );
		break;

	case FIELD_GLOBAL_POSITION:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_log_position.asint );
		break;

	case FIELD_GLOBAL_RECORD_NUM:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_log_record.asint );
		break;

	case FIELD_INODE:
		printf( "  %s: %lu\n", wdata->m_name,
				(long unsigned) istate->m_inode );
		break;

	case FIELD_CTIME:
		if ( opts.getNumeric() ) {
			printf( "  %s: %lu\n", wdata->m_name,
					(long unsigned) istate->m_ctime );
		} else {
			printf( "  %s: '%s'\n", wdata->m_name,
					timestr(istate->m_ctime) );
		}
		break;

	case FIELD_SIZE:
		printf( "  %s: " FILESIZE_T_FORMAT "\n",
				wdata->m_name, istate->m_size.asint );
		break;

	case FIELD_ALL:
		DumpState( opts, state, opts.lookupField(FIELD_SIGNATURE) );
		DumpState( opts, state, opts.lookupField(FIELD_VERSION) );
		DumpState( opts, state, opts.lookupField(FIELD_UPDATE_TIME) );
		DumpState( opts, state, opts.lookupField(FIELD_BASE_PATH) );
		DumpState( opts, state, opts.lookupField(FIELD_CUR_PATH) );
		DumpState( opts, state, opts.lookupField(FIELD_UNIQ_ID) );
		DumpState( opts, state, opts.lookupField(FIELD_SEQUENCE) );
		DumpState( opts, state, opts.lookupField(FIELD_MAX_ROTATION) );
		DumpState( opts, state, opts.lookupField(FIELD_ROTATION) );
		DumpState( opts, state, opts.lookupField(FIELD_OFFSET) );
		DumpState( opts, state, opts.lookupField(FIELD_EVENT_NUM) );
		DumpState( opts, state, opts.lookupField(FIELD_GLOBAL_POSITION) );
		DumpState( opts, state, opts.lookupField(FIELD_GLOBAL_RECORD_NUM) );
		DumpState( opts, state, opts.lookupField(FIELD_INODE) );
		DumpState( opts, state, opts.lookupField(FIELD_CTIME) );
		DumpState( opts, state, opts.lookupField(FIELD_SIZE) );
		break;

	default:
		return -1;
	}
	return 0;
}

int
CheckStateAccess( const Options & /*opts*/,
				  const ReadUserLog::FileState &state )
{
	ReadUserLogStateAccess	saccess(state);
	unsigned long			foff = 0;
	unsigned long			fnum = 0;
	unsigned long			lpos = 0;
	unsigned long			lnum = 0;
	int						seq = 0;
	char					uniq_id[256] = "\0";
	int						errors = 0;

	if (!saccess.getFileOffset(foff) ) {
		fprintf( stderr, "Error getting file offset\n" );
		errors++;
	}
	if (!saccess.getFileEventNum(fnum) ) {
		fprintf( stderr, "Error getting file event number\n" );
		errors++;
	}
	if (!saccess.getLogPosition(lpos) ) {
		fprintf( stderr, "Error getting log position\n" );
		errors++;
	}
	if (!saccess.getEventNumber(lnum) ) {
		fprintf( stderr, "Error getting event number\n" );
		errors++;
	}
	if (!saccess.getUniqId(uniq_id, sizeof(uniq_id)) ) {
		fprintf( stderr, "Error getting uniq ID\n" );
		errors++;
	}
	if (!saccess.getSequenceNumber(seq) ) {
		fprintf( stderr, "Error getting sequence number\n" );
		errors++;
	}

	printf( "State:\n"
			"  Initialized: %s\n"
			"  Valid: %s\n"
			"  File offset: %lu\n"
			"  File event #: %lu\n"
			"  Log position: %lu\n"
			"  Log event #: %lu\n"
			"  UniqID: %s\n"
			"  Sequence #: %d\n",
			saccess.isInitialized() ? "True" : "False",
			saccess.isValid() ? "True" : "False",
			foff,
			fnum,
			lpos,
			lnum,
			uniq_id,
			seq );

	return errors ? -1 : 0;
}

int
DiffState( const Options &  /*opts*/,
		   const ReadUserLog::FileState &state1,
		   const ReadUserLog::FileState &state2 )
{
	ReadUserLogStateAccess	access1(state1);
	ReadUserLogStateAccess	access2(state2);
	int						errors = 0;
	
	return errors ? -1 : 0;
}

bool
CompareStr( const Options &opts, const char *val )
{
	bool	ok;

	ok = ( strcasecmp( val, opts.getValue().asStr ) == 0 );
	if ( !ok && opts.getVerbose() ) {
		printf( "  %s: '%s' != '%s'\n",
				opts.getField()->m_name, val, opts.getValue().asStr );
	}
	return ok;
}

bool
CompareInt( const Options &opts, int val )
{
	bool	ok;

	if ( opts.isValueRange() ) {
		ok = (  ( val >= opts.getValue().asRange.minVal.asInt ) &&
				( val <= opts.getValue().asRange.maxVal.asInt )  );

		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: %d not in %d - %d\n",
					opts.getField()->m_name,
					val,
					opts.getValue().asRange.minVal.asInt,
					opts.getValue().asRange.maxVal.asInt );
		}
	}
	else {
		ok = ( val == opts.getValue().asInt.asInt );
		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: %d != %d\n",
					opts.getField()->m_name,
					val,
					opts.getValue().asInt.asInt );
		}
	}

	return ok;
}

bool
CompareFsize( const Options &opts, filesize_t val )
{
	bool	ok;

	if ( opts.isValueRange() ) {
		ok = (  ( val >= opts.getValue().asRange.minVal.asFsize ) &&
				( val <= opts.getValue().asRange.maxVal.asFsize )  );

		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: " FILESIZE_T_FORMAT
					"not in " FILESIZE_T_FORMAT " - " FILESIZE_T_FORMAT "\n",
					opts.getField()->m_name,
					val,
					opts.getValue().asRange.minVal.asFsize,
					opts.getValue().asRange.maxVal.asFsize );
		}
	}
	else {
		ok = ( val == opts.getValue().asInt.asInt );
		if ( !ok && opts.getVerbose() ) {
			printf( "  %s: " FILESIZE_T_FORMAT " != " FILESIZE_T_FORMAT "\n",
					opts.getField()->m_name,
					val,
					opts.getValue().asInt.asFsize );
		}
	}

	return ok;
}

bool
CompareTime( const Options &opts, time_t val )
{
	bool	ok;
	char	b1[64], b2[64], b3[64];

	if ( opts.isValueRange() ) {
		ok = (  ( val >= opts.getValue().asRange.minVal.asTime ) &&
				( val <= opts.getValue().asRange.maxVal.asTime )  );

		if ( !ok && opts.getVerbose() ) {
			if ( opts.getNumeric() ) {
				printf( "  %s: %lu not in %lu - %lu\n",
						opts.getField()->m_name,
						(unsigned long)val,
						(unsigned long)opts.getValue().asRange.minVal.asTime,
						(unsigned long)opts.getValue().asRange.maxVal.asTime );
			}
			else {
				printf( "  %s: %s not in %s - %s\n",
						opts.getField()->m_name,
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
						opts.getField()->m_name,
						(unsigned long) val,
						(unsigned long) opts.getValue().asInt.asTime );
			}
			else {
				printf( "  %s: %s != %s\n",
						opts.getField()->m_name,
						timestr( val, b1, sizeof(b1) ),
						timestr( opts.getValue().asRange.minVal.asTime,
								 b2, sizeof(b2) ) );
			}
		}
	}

	return ok;
}

bool
CompareInode( const Options &opts, StatStructInode val )
{
	bool	ok;

	ok = val == opts.getValue().asInode;
	if ( !ok && opts.getVerbose() ) {
			printf( "  %s: %lu != %lu\n",
					opts.getField()->m_name,
					(unsigned long) val,
					(unsigned long) opts.getValue().asInode );
	}

	return ok;
}

int
VerifyState(const Options &opts, const ReadUserLog::FileState &state )
{
	ReadUserLogState	rstate( state, 60 );
	const ReadUserLogState::FileState	*istate;
	ReadUserLogState::convertState( state, istate );

	const FieldData	*wdata = opts.getField( );
	if ( wdata == NULL ) {
		fprintf( stderr, "Verify: no field!\n" );
		return -1;
	}

	bool	ok;
	switch( wdata->m_field )
	{
	case FIELD_SIGNATURE:
		ok = CompareStr( opts, istate->m_signature );
		break;

	case FIELD_VERSION:
		ok = CompareInt( opts, istate->m_version );
		break;

	case FIELD_UPDATE_TIME:
		ok = CompareTime( opts, istate->m_update_time );
		break;

	case FIELD_BASE_PATH:
		ok = CompareStr( opts, istate->m_base_path );
		break;

	case FIELD_CUR_PATH:
		ok = CompareStr( opts, rstate.CurPath(state) );
		break;

	case FIELD_UNIQ_ID:
		ok = CompareStr( opts, istate->m_uniq_id );
		break;

	case FIELD_SEQUENCE:
		ok = CompareInt( opts, istate->m_sequence );
		break;

	case FIELD_MAX_ROTATION:
		ok = CompareInt( opts, istate->m_max_rotations );
		break;

	case FIELD_ROTATION:
		ok = CompareInt( opts, istate->m_rotation );
		break;

	case FIELD_OFFSET:
		ok = CompareFsize( opts, istate->m_offset.asint );
		break;

	case FIELD_EVENT_NUM:
		ok = CompareFsize( opts, istate->m_event_num.asint );
		break;

	case FIELD_GLOBAL_POSITION:
		ok = CompareFsize( opts, istate->m_log_position.asint );
		break;

	case FIELD_GLOBAL_RECORD_NUM:
		ok = CompareFsize( opts, istate->m_log_record.asint );
		break;

	case FIELD_INODE:
		ok = CompareInode( opts, istate->m_inode );
		break;

	case FIELD_CTIME:
		ok = CompareTime( opts, istate->m_ctime );
		break;

	case FIELD_SIZE:
		ok = CompareFsize( opts, istate->m_size.asint );
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
timestr( struct tm &t, char *buf, int bufsize )
{
	static char	sbuf[64];
	if ( NULL == buf ) {
		buf = sbuf;
		bufsize = sizeof(sbuf);
	}

	strncpy( buf, asctime( &t ), bufsize );
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
	m_state = ST_CMD;
	m_file = NULL;
	m_file2 = NULL;
	m_command = CMD_NONE;
	m_field = NULL;
	memset( &m_value, 0, sizeof(m_value) );
	m_value_is_range = false;
	m_num_values = 0;
	m_verbose = VERB_NONE;
	m_numeric = false;
	m_usage =
		"Usage: test_log_reader_state "
		"[options] <command> [filename [field-name [value/min [max-value]]]]\n"
		"  commands: dump diff verify check list\n"
		"    dump/any: dump [what]\n"
		"    diff: diff two state files [file2]\n"
		"    verify/numeric: what min max\n"
		"    verify/string:  what [value]\n"
		"    access: simple access API checks\n"
		"    list (list names of things)\n"
		"  --numeric|-n: dump times as numeric\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n"
		"  <filename>: the persistent file to read\n";
}

int
Options::handleOpt( SimpleArg &arg, int &argno )
{
	if ( arg.Match('n', "numeric") ) {
		m_numeric = true;

	} else if ( arg.Match('d', "debug") ) {
		if ( arg.hasOpt() ) {
			set_debug_flags( const_cast<char *>(arg.getOpt()), 0 );
			argno = arg.ConsumeOpt( );
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
		printf("test_log_reader_state: %s\n", VERSION);
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
Options::handleFixed( SimpleArg &arg, int & /*argno*/ )
{
	if ( ST_CMD == m_state ) {
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
			m_field = lookupField( "all" );
			if ( NULL == m_field ) {
				assert( 0 );
			}
		}
		m_state = ST_FILE;
	}
	else if ( ST_FILE == m_state ) {
		m_file = arg.Arg();
		m_state = ST_FIELD;
	}
	else if ( ST_FIELD == m_state ) {
		m_field = lookupField( arg );
		if ( NULL == m_field ) {
			fprintf(stderr, "Invalid field '%s'\n", arg.Arg() );
			printf("%s", m_usage);
			m_state = ST_NONE;
			return -1;
		}

		if ( 0 == m_field->m_max_values ) {
			m_state = ST_NONE;
			return 1;
		}
		m_state = ST_VALUES;
	}
	else if (  ( ST_VALUES == m_state ) &&
			   ( m_field ) &&
			   ( m_num_values < m_field->m_max_values ) ) {
		if ( !parseValue( arg ) ) {
			fprintf(stderr, "Invalid value for %s: '%s'\n",
					m_field->m_name, arg.Arg() );
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
	else if ( 0 == strcasecmp( s, "diff" ) ) {
		m_command = CMD_DIFF;
		return true;
	}
	else if ( 'v' == c  ||  (0 == strcasecmp( s, "verify" )) ) {
		m_command = CMD_VERIFY;
		return true;
	}
	else if ( 'a' == c  ||  (0 == strcasecmp( s, "access" )) ) {
		m_command = CMD_ACCESS;
		return true;
	}
	else if ( 'l' == c  ||  (0 == strcasecmp( s, "list" )) ) {
		m_command = CMD_LIST;
		return true;
	}
	return false;
}

void
Options::dumpFieldList( void ) const
{
	const FieldData	*field = &fieldList[0];
	for( field = &fieldList[0]; field->m_field != FIELD_NONE; field++ ) {
		printf( "  %s\n", field->m_name );
	}
}

const FieldData *
Options::lookupField( Field arg ) const
{
	const FieldData	*field;
	for( field = &fieldList[0]; field->m_field != FIELD_NONE; field++ ) {
		if ( field->m_field == arg ) {
			return field;
		}
	}
	return NULL;
}

const FieldData *
Options::lookupField( const char *arg ) const
{
	const FieldData	*field;
	for( field = &fieldList[0]; field->m_field != FIELD_NONE; field++ ) {
		if ( 0 == strcasecmp( arg, field->m_name ) ) {
			return field;
		}
	}
	return NULL;
}

const FieldData *
Options::lookupField( const SimpleArg &arg ) const
{
	return lookupField( arg.Arg() );
}

bool
Options::parseValue( const SimpleArg &arg )
{
	const char *s = arg.Arg( );
	if ( DTYPE_STRING == m_field->m_type ) {
		m_value.asStr = s;
		return true;
	}
	if ( ( DTYPE_INT == m_field->m_type ) && ( isdigit(*s) ) ) {
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
	if ( ( DTYPE_FSIZE == m_field->m_type ) && ( isdigit(*s) ) ) {
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
	if ( ( DTYPE_TIME == m_field->m_type ) && ( isdigit(*s) ) ) {
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
	if ( ( DTYPE_INODE == m_field->m_type ) && ( isdigit(*s) ) ) {
		m_value.asInode = (StatStructInode) atol(s);
		return true;
	}
	return false;
}
