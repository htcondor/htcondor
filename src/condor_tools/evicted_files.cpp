#include "condor_common.h"
#include "condor_config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "my_popen.h"
#include "directory.h"
#include "spooled_job_files.h"

#include <vector>

typedef std::pair< int, int > JobID;
typedef std::vector< JobID > JobIDList;

bool makeJobID( const char * c, JobID & j ) {
    int clusterID, procID;
    if( sscanf( c, "%d.%d", & clusterID, & procID ) != 2 ) {
        return false;
    }
    j = std::make_pair( clusterID, procID );
    return true;
}

void usage( const char * zero) {
    fprintf( stdout, "Usage: %s [COMMAND] (clusterID.procID)+\n", zero );
    fprintf( stdout, "Deal with evicted files.\n" );
    fprintf( stdout, "\n" );
    fprintf( stdout, "With no COMMAND, assume DIR.  Otherwise:\n" );
    fprintf( stdout, "\tdir\tPrint the directory in which the files are stored.\n" );
    fprintf( stdout, "\tlist\tList the evicted files.\n" );
    fprintf( stdout, "\tget\tCopy the evicted files to a subdirectory named after the job ID.\n" );
}

int main( int argc, char ** argv ) {
    if( argc < 2 ) { usage( argv[0] ); return -1; }
    config();

    enum struct Command {
        dir = 1,
        list = 2,
        get = 3
    };

    int firstJobID = 1;
    Command c = Command::dir;
    if(! isdigit( argv[firstJobID][0] )) {
        firstJobID = 2;

        if( strcasecmp( argv[1], "dir" ) == 0 ) { c = Command::dir; }
        else if( strcasecmp( argv[1], "find" ) == 0 ) { c = Command::dir; }
        else if( strcasecmp( argv[1], "ls" ) == 0 ) { c = Command::list; }
        else if( strcasecmp( argv[1], "list" ) == 0 ) { c = Command::list; }
        else if( strcasecmp( argv[1], "get" ) == 0 ) { c = Command::get; }
        else if( strcasecmp( argv[1], "fetch" ) == 0 ) { c = Command::get; }
        else {
            fprintf( stdout, "Unknown command '%s', aborting.\n", argv[1] );
            return -1;
        }
    }

    JobIDList jobIDs;
    for( int i = firstJobID; i < argc; ++i ) {
        JobID jobID;
        if( makeJobID(argv[i], jobID) ) {
            jobIDs.push_back( jobID );
        } else {
            fprintf( stderr, "'%s' is not a job ID, aborting.\n", argv[i] );
            return -1;
        }
    }

    bool single = jobIDs.size() == 1;
    for( const auto & jobID : jobIDs ) {
        std::string path;
        SpooledJobFiles::getJobSpoolPath( jobID.first, jobID.second, path );
        // FIXME: Handles ALTERNATE_JOB_SPOOL.
        // SpooledJobFiles::getJobSpoolPath( jobAd, path );

        Directory d( path.c_str() );
        if(! d.Rewind()) {
            if(! single) { fprintf( stdout, "%d.%d ", jobID.first, jobID.second ); }

            if( errno == ENOENT ) {
                fprintf( stdout, "No directory; job was never evicted.\n" );
            } else {
                fprintf( stdout, "[error %d: '%s' while looking for '%s']\n", errno, strerror(errno), path.c_str() );
            }

            if( single ) { return errno; }
            continue;
        }

        if(! single) { fprintf( stdout, "%d.%d ", jobID.first, jobID.second ); }

        switch(c) {
            case Command::dir:
                fprintf( stdout, "%s\n", path.c_str() );
                break;

            case Command::list: {
                if(! single) { fprintf( stdout, "\n" ); }
                const char * de_name = NULL;
                while( (de_name = d.Next()) != NULL ) {
                    if( strcmp( ".", de_name ) == 0 ) { continue; }
                    if( strcmp( "..", de_name ) == 0 ) { continue; }
                    fprintf( stdout, "\t%s\n", de_name );
                }
                } break;

            case Command::get: {
                std::string dest;
                formatstr( dest, "%d.%d", jobID.first, jobID.second );
                Directory destd( dest.c_str() );
                if( destd.Rewind() ) {
                    fprintf( stdout, "Destination directory '%s' exists, not modifying.\n", dest.c_str() );
                    if( single ) { return -1; }
                    continue;
                }

                ArgList args;
#if defined(WINDOWS)
                args.AppendArg( "xcopy" );
                args.AppendArg( "/S" );
                args.AppendArg( "/E" );
                args.AppendArg( "/K" );
                args.AppendArg( path.c_str() );
                // It's important that dest have a trailing \.
                std::string target;
                formatstr( target, "%s\\", dest.c_str() );
                args.AppendArg( target.c_str() );
#else
                args.AppendArg( "cp" );
                args.AppendArg( "-a" );
                // It's important that path and dest not have trailing /s.
                args.AppendArg( path.c_str() );
                args.AppendArg( dest.c_str() );
#endif /* defined(WINDOWS) */

                int r = -1;
                FILE * f = my_popen( args, "r", MY_POPEN_OPT_WANT_STDERR );
                if( f ) {
                    r = my_pclose(f);
                }

                if( r == 0 ) {
                    fprintf( stdout, "Copied to '%s'.\n", dest.c_str() );
                } else {
                    fprintf( stdout, "Copy failed: return value %d.\n", r );
                }
                if( single ) { return r; }
                } break;
        }
    }

    return 0;
}
