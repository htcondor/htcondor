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
#include "stat_wrapper.h"

struct StatOp {
	const char 				*str;
	StatWrapper::StatOpType	 type;
};

static void Help( const char *argv0 );
static const StatOp *get_op( const char *name );
static const char *get_op_name( StatWrapper::StatOpType which );
static void dump_status( StatWrapper &stat, const char *label );

static const StatOp Ops[] = {
	{ "NONE",	StatWrapper::STATOP_NONE },
	{ "STAT",	StatWrapper::STATOP_STAT },
	{ "LSTAT",	StatWrapper::STATOP_LSTAT },
	{ "BOTH",	StatWrapper::STATOP_BOTH },
	{ "FSTAT",	StatWrapper::STATOP_FSTAT },
	{ "ALL",	StatWrapper::STATOP_ALL },
	{ "LAST",	StatWrapper::STATOP_LAST },
	{ NULL,		(StatWrapper::StatOpType) -1 },
};

int
main( int argc, const char *argv[] )
{
	int				 fd;
	const char		*path;
	StatWrapper		*wrapper = NULL;
	const char	*usage =
		"test [-h|--help] <file> [operations]";

	if ( argc < 2 ) {
		fprintf( stderr, "no file specified\n" );
		fprintf( stderr, "%s\n", usage );
		exit( 1 );
	}
	path = argv[1];
	if ( *path == '-' ) {
		fprintf( stderr, "no file specified\n" );
		fprintf( stderr, "%s\n", usage );
		Help( argv[0] );
		exit( 1 );
	}
	fd = safe_open_wrapper( path, O_RDONLY );
	printf( "path set to '%s', fd to %d\n", path, fd );

	int		argno;
	int		skip = 0;
	for( argno = 2; argno < argc;  argno++ ) {
		if ( skip ) {
			skip--;
			continue;
		}
		const char	*arg = argv[argno];
		const char	*arg1 = NULL;
		const char	*arg2 = NULL;
		const char	*arg3 = NULL;
		if ( arg  && (argc > (argno+1)) && (*argv[argno+1] != '-')  ) {
			arg1 = argv[argno+1];
		}
		if ( arg1 && (argc > (argno+2)) && (*argv[argno+2] != '-')  ) {
			arg2 = argv[argno+2];
		}
		if ( arg2 && (argc > (argno+3)) && (*argv[argno+3] != '-')  ) {
			arg3 = argv[argno+3];
		}

		if ( (!strcmp( arg, "--help" )) || (!strcmp( arg, "-h")) ) {
			Help( argv[0] );
			exit( 0 );
		}

		else if ( (!strcmp( arg, "--path" )) || (!strcmp( arg, "-p")) ) {
			if ( !arg1 ) {
				fprintf( stderr, "--path usage: --path <path>\n" );
				exit( 1 );
			}
			path = arg1;
			fd = safe_open_wrapper( path, O_RDONLY );
			skip = 1;
			printf( "path set to '%s', fd to %d\n", path, fd );
		}

		else if ( (!strcmp( arg, "--create" )) || (!strcmp( arg, "-c")) ) {

			if ( !arg1 || !arg2 ) {
				fprintf( stderr, "--create usage: --create <op> <which>\n" );
				exit( 1 );
			}

			skip = 2;
			const StatOp *op    = get_op( arg1 );
			const StatOp *which = get_op( arg1 );
			const char *n = "";
			bool auto_stat = false;

			// Create the object
			switch ( op->type ) {
			case StatWrapper::STATOP_STAT:
			case StatWrapper::STATOP_LSTAT:
			case StatWrapper::STATOP_BOTH:
				n = "StatWrapper(path,which)";
				auto_stat = ( (which->type==StatWrapper::STATOP_STAT)  ||
							  (which->type==StatWrapper::STATOP_LSTAT) ||
							  (which->type==StatWrapper::STATOP_BOTH)  ||
							  (which->type==StatWrapper::STATOP_ALL)   );
				wrapper = new StatWrapper( path, which->type );
				break;
			case StatWrapper::STATOP_FSTAT:
				n = "StatWrapper(fd,which)";
				wrapper = new StatWrapper( fd, which->type );
				auto_stat = ( (which->type==StatWrapper::STATOP_FSTAT) ||
							  (which->type==StatWrapper::STATOP_ALL)   );
				break;
			case StatWrapper::STATOP_NONE:
				n = "StatWrapper(void)";
				wrapper = new StatWrapper;
				break;
			case StatWrapper::STATOP_ALL:
			case StatWrapper::STATOP_LAST:
			default:
				fprintf( stderr, "%s (%d) doesn't make sense for create\n",
						 op->str, op->type );
				exit( 1 );
				break;
			}
			if ( !wrapper ) {
				fprintf( stderr,
						 "Failed to create StatWrapper (%s) object!\n", n );
				exit( 1 );
			}
			printf( "Created StatWrapper object via %s\n", n );

			printf( "Stat Functions:\n" );
			int		opint;
			for( opint = (int)StatWrapper::STATOP_MIN;
				 opint <= (int)StatWrapper::STATOP_MAX;
				 opint++ ) {
				StatWrapper::StatOpType	opno = (StatWrapper::StatOpType) opint;
				const char	*fn = wrapper->GetStatFn( opno );
				if ( NULL == fn ) {
					fn = "<NULL>";
				}
				const char	*name = get_op_name( opno );
				printf( "  %s = %s\n", name, fn );
			}

			if ( auto_stat ) {
				dump_status( *wrapper, "Stat results" );
			}
		}

		else if ( !strcmp( arg, "--set" ) ) {
			skip = 1;

			if ( !arg1 ) {
				fprintf( stderr, "--set usage: --set <op>\n" );
				exit( 1 );
			}
			const StatOp *op = get_op( arg1 );

			if ( !wrapper ) {
				fprintf( stderr, "--set: no wrapper object!\n" );
				exit( 1 );
			}

			// Set operations
			bool rc1 = false, rc2 = false;
			const char *n1 = "", *n2 = "";
			switch ( op->type ) {
			case StatWrapper::STATOP_STAT:
			case StatWrapper::STATOP_LSTAT:
			case StatWrapper::STATOP_BOTH:
				n1 = "SetPath";
				rc1 = wrapper->SetPath( path );
				break;
			case StatWrapper::STATOP_FSTAT:
				n1 = "SetFd";
				rc1 = wrapper->SetFD( fd );
				break;
			case StatWrapper::STATOP_NONE:
				break;
			case StatWrapper::STATOP_ALL:
				n1 = "SetPath";
				rc1 = wrapper->SetPath( path );
				n2 = "SetFD";
				rc2 = wrapper->SetFD( fd );
				break;
			case StatWrapper::STATOP_LAST:
			default:
				fprintf( stderr, "%s (%d) doesn't make sense for set\n",
						 op->str, op->type );
				exit( 1 );
				break;
			}
			if ( rc1 || rc2 ) {
				printf( "set via %s [%s/%s] OK\n",
						op->str, n1, n2 );
			} else {
				fprintf( stderr, "set via %s [%s/%s] FAILED\n",
						 op->str, n1, n2 );
				exit( 1 );
			}
		}

		else if ( (!strcmp( arg, "--stat" )) || (!strcmp( arg, "-s")) ) {
			if ( !arg2 ) {
				fprintf( stderr,
						 "--stat usage: --stat <op> <which> [f[orce]|[no]]\n");
				exit( 1 );
			}
			const StatOp *op    = get_op( arg1 );
			const StatOp *which = get_op( arg2 );
			bool force = true;
			bool force_set;

			if ( arg3 ) {
				skip = 3;
				force = (  (!strcasecmp(arg3,"f"))  ||
						   (!strcasecmp(arg3,"force")) );
			}
			else {
				skip = 2;
				force_set = false;
			}

			if ( !wrapper ) {
				fprintf( stderr, "--stat: no wrapper object!\n" );
				exit( 1 );
			}

			// Stat operations
			int		rc1 = 0, rc2 = 0;
			bool	op2 = false;
			char	*n1 = "", *n2 = "";
			switch ( op->type ) {
			case StatWrapper::STATOP_NONE:
			case StatWrapper::STATOP_LAST:
				if ( force_set ) {
					n1 = "Stat(which,force)";
					rc1 = wrapper->Stat( which->type, force );
				} else {
					n1 = "Stat(which)";
					rc1 = wrapper->Stat( which->type );
				}
				break;
			case StatWrapper::STATOP_STAT:
			case StatWrapper::STATOP_LSTAT:
			case StatWrapper::STATOP_BOTH:
				if ( force_set ) {
					n1 = "Stat(path,which,force)";
					rc1 = wrapper->Stat( path, which->type, force );
				} else {
					n1 = "Stat(path,which)";
					rc1 = wrapper->Stat( path, which->type );
				}
				break;
			case StatWrapper::STATOP_FSTAT:
				if ( force_set ) {
					n1 = "Stat(fd,force)";
					rc1 = wrapper->Stat( fd, which->type, force );
				} else {
					n1 = "Stat(fd)";
					rc1 = wrapper->Stat( fd );
				}
				break;
			case StatWrapper::STATOP_ALL:
				if ( force_set ) {
					n1 = "Stat(path,which,force)";
					rc1  = wrapper->Stat( path, which->type, force );
					n2 = "Stat(fd,force)";
					rc2 = wrapper->Stat( fd, force );
				} else {
					n1 = "Stat(path,which)";
					rc1  = wrapper->Stat( path, which->type );
					n2 = "Stat(fd)";
					rc2 = wrapper->Stat( fd );
				}
				op2 = true;
				break;
			default:
				fprintf( stderr, "%s (%d) doesn't make sense for create\n",
						 op->str, op->type );
			}
			printf( "%s %s (%d)\n", n1, rc1 ? "Failed":"OK", rc1 );
			if ( op2 ) {
				printf( "%s %s (%d)\n", n2, rc2 ? "Failed":"OK", rc2 );
			}	

			dump_status( *wrapper, "Stat results" );
		}

		else if ( (!strcmp( arg, "--retry" )) || (!strcmp( arg, "-r")) ) {
			if ( !wrapper ) {
				fprintf( stderr, "--retry: no wrapper object!\n" );
				exit( 1 );
			}

			int		rc = wrapper->Retry( );
			printf( "Retry %s: %d\n", rc ? "Failed":"OK", rc );

			dump_status( *wrapper, "Retry results" );
		}

		else if ( (!strcmp( arg, "--query" )) || (!strcmp( arg, "-q")) ) {
			if ( !wrapper ) {
				fprintf( stderr, "--query: no wrapper object!\n" );
				exit( 1 );
			}

			dump_status( *wrapper, "Query results" );
		}

		else {
			fprintf( stderr, "Unknown command %s\n", arg );
		}
	}

	return 0;
}

static void
Help( const char *argv0 )
{
	printf( "usage: %s <file> [-command <flags>] [..]\n"
			"  commands:\n"
			"  --create|-c <op> <which>         "
			"Create a StatWrapper object\n"
			"  --set|-c <op>                    "
			"Setup a StatWrapper object\n"
			"  --stat|-s <op> <which> [f[orce]|no]"
			"Do the actual stat (optional force) \n"
			"  --retry                          "
			"Retry last operation\n"
			"  --query                          "
			"Query status of object\n"
			"  <op>: Type of operation:         "
			"NONE,STAT,LSTAT,BOTH,FSTAT,ALL,LAST\n"
			"  <which>: Which for operation:    "
			"NONE,STAT,LSTAT,BOTH,FSTAT,ALL,LAST\n"
			"  --path <path>                    "
			"  <path> specify a new path\n"
			"",
			argv0
			);
};

static void
dump_status( StatWrapper &stat, const char *label )
{
	int	opint;
	for( opint = StatWrapper::STATOP_MIN;
		 opint <= StatWrapper::STATOP_MAX;
		 opint++ ) {

		StatWrapper::StatOpType	opno = (StatWrapper::StatOpType) opint;

		const char	*fn = stat.GetStatFn( opno );
		if ( NULL == fn ) {
			fn = "<NULL>";
		}
		const char		*name = get_op_name( opno );
		StatStructType	 buf;
		int				 err = 0;
		bool			 valid = false;
		bool			 getbuf = false;

		int rc = stat.GetRc( opno );
		if ( rc ) {
			err = stat.GetErrno( opno );
		}
		valid = stat.IsBufValid( opno );
		if( valid ) {
			getbuf = stat.GetBuf( buf, opno );
		}

		printf( "  %s %s: fn=%s; rc=%d errno=%d valid=%s getbuf=%s\n",
				label, name, fn, rc, err,
				valid ? "true":"false", getbuf ? "true":"false" );
		if ( valid ) {
			printf( "    buf: dev=%d ino=%u mode=%03o nlink=%d ids=%d/%d\n"
					"         size=%lld a/m/ctime=%d/%d/%d\n",
					(int)buf.st_dev,
					(unsigned)buf.st_ino, (unsigned)buf.st_mode,
					(int)buf.st_nlink,
					(int)buf.st_uid, (int)buf.st_gid,
					(long long)buf.st_size,
					(int)buf.st_atime, (int)buf.st_mtime, (int)buf.st_ctime );
		}
	}
}

static const StatOp *
get_op( const char *str )
{

	if ( NULL == str ) {
		return &Ops[0];
	}

	for( const StatOp *op = Ops; op->str;  op++ ) {
		if ( !strcasecmp( op->str, str ) ) {
			return op;
		}
	}
	return &Ops[0];
};

static const char *
get_op_name( StatWrapper::StatOpType which )
{
	if ( ( which < StatWrapper::STATOP_MIN ) ||
		 ( which > StatWrapper::STATOP_MAX ) ) {
		return "NONE";
	}
	return Ops[which].str;
};
