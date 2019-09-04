#include "condor_common.h"
#include "condor_config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "my_popen.h"

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

    // This is the tool's view of SPOOL.  Even if this tool never works for
    // remote submission, this should still, instead, be asking the schedd
    // what it thinks the SPOOL directory is, since that's where the file(s),
    // if any, will actually be.
    config();

    std::string spool;
    if(! param( spool, "SPOOL" )) {
        fprintf( stderr, "SPOOL not defined, aborting.\n" );
        return -2;
    }

    enum struct Command {
        none = 0,
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

    if( c == Command::none ) {
        usage( argv[0] );
        return -1;
    }


    bool single = jobIDs.size() == 1;
    for( const auto & jobID : jobIDs ) {
        std::string path;
#if defined(WINDOWS)
        formatstr( path, "%s\\%d\\%d\\cluster%d.proc%d.subproc0",
            spool.c_str(), jobID.first, jobID.second,
            jobID.first, jobID.second );
#else
        formatstr( path, "%s/%d/%d/cluster%d.proc%d.subproc0",
            spool.c_str(), jobID.first, jobID.second,
            jobID.first, jobID.second );
#endif /* defined(WINDOWS) */

        DIR * d = opendir( path.c_str() );
        if( d == NULL ) {
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

        struct dirent * de = NULL;
        switch(c) {
            case Command::dir:
                fprintf( stdout, "%s\n", path.c_str() );
                break;

            case Command::list:
                fprintf( stdout, "\n" );
                while( (de = readdir(d)) != NULL ) {
                    if( strcmp( ".", de->d_name ) == 0 ) { continue; }
                    if( strcmp( "..", de->d_name ) == 0 ) { continue; }
                    fprintf( stdout, "\t%s\n", de->d_name );
                }
                break;

            case Command::get: {
                std::string dest;
                formatstr( dest, "%d.%d", jobID.first, jobID.second );
                DIR * destd = opendir( dest.c_str() );
                if( destd != NULL ) {
                    closedir( destd );
                    fprintf( stdout, "Destination directory '%s' exists, not modifying.\n", dest.c_str() );
                    if( single ) { return -1; }
                    continue;
                }

                ArgList args;
#if defined(WINDOWS)
                args.AppendArg( "xcopy" );
                args.AppendArg( "/S" );
                args.AppendArg( "/E" );
                args.Appendarg( "/K" );
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

            case Command::none:
                // The compiler should know better.
                break;
        }

        closedir( d );
    }

    return 0;
}
