#include "condor_common.h"
#include "condor_config.h"

#include "shortfile.h"
#include "safe_open.h"

#include <filesystem>
#include <functional>
#include <fstream>

bool
dumpFileToStandardOut(
  const std::filesystem::path & path,
  std::function<bool(const char *)> filter
) {
    std::ifstream i( path );
    if(! i.good()) {
        fprintf( stderr, "Failed to open %s\n", path.string().c_str() );
        return false;
    }

    std::string line;
    while( std::getline( i, line ).good() ) {
        if( filter( line.c_str() ) ) {
            fprintf( stdout, "%s\n", line.c_str() );
        }
    }
    if(! i.eof()) { return false; }

    return true;
}


// We can't just check for `pattern`, because that will miss lines from the
// FTO if it's a forked child.  Instead, we look for the first banner line;
// if it also contains `pattern`, we start including log lines.  We stop
// including log lines when we find a first banner line that doesn't
// contain `pattern` (note that we can't check for this _before_ we find
// our own first banner line, because there are other starters in the log).
//
// As this function is currently called, we'll never see the next starter's
// banner (because this starter hasn't exited yet).
//
bool
relevantPartOfLog( const std::string & pattern, const char * line ) {
    static bool saw_our_banner_line = false;
    static bool saw_bad_banner_line = false;

    if( NULL != strstr( line, "******************************************************" )) {
        bool good_pid = NULL != strstr( line, pattern.c_str() );

        if(! saw_our_banner_line) {
            saw_our_banner_line = good_pid;
        } else {
            saw_bad_banner_line = (! good_pid);
        }
    }

    if( saw_our_banner_line && ! saw_bad_banner_line ) {
        return true;
    } else {
        return false;
    }
}

bool
printGlobalStarterLog( pid_t pid ) {
    //
    // Acquire the LOG directory.
    //
    std::string LOG;
    if(! param(LOG, "LOG")) {
        fprintf( stderr, "HTCondor configuration value LOG not defined.\n" );
        return false;
    }


    //
    // Acquire NAME from the .machine.ad file.
    //
    FILE * file = safe_fopen_no_create( "./.machine.ad", "r" );
    if( file == NULL ) {
        fprintf( stderr, "Failed to open .machine.ad.\n" );
        return false;
    }

    CondorClassAdFileIterator ccafi;
    if(! ccafi.begin( file, true, CondorClassAdFileParseHelper::Parse_long )) {
        fprintf( stderr, "Failed to start machine ad.\n" );
        return false;
    }

    ClassAd machineAd;
    if( ccafi.next( machineAd ) < 0 ) {
        fprintf( stderr, "Failed to parse machine ad.\n" );
        return false;
    }

    std::string NAME;
    if(! machineAd.LookupString( "Name", NAME )) {
        fprintf( stderr, "Machine ad did not contain Name attribute.\n" );
        return false;
    }

    size_t at = NAME.find_first_of('@');
    if( at == std::string::npos ) {
        fprintf( stderr, "Name attribute did not contain @.\n" );
        return false;
    }
    NAME.erase( NAME.find_first_of('@') );


    //
    //  Dump the global log to standard out.
    //
    std::string pattern;
    formatstr( pattern, "(pid:%d)", pid );
    std::filesystem::path logPath(LOG);
    return dumpFileToStandardOut(
        logPath / ("StarterLog." + NAME),
        [=] (const char * line) -> bool { return relevantPartOfLog( pattern, line ); }
    );
}


bool
all_lines( const char * /* line */ ) {
    return true;
}


bool
printLocalStarterLog() {
    return dumpFileToStandardOut( "./.starter.log", all_lines );
}


int
main( int /* argc */, char ** /* argv */ ) {
    config();

    // On Windows, there's no getppid(), so the starter passes it
    // through the environment.
    const char * _condor_starter_pid = getenv("_CONDOR_STARTER_PID");
    if( _condor_starter_pid == NULL ) {
        fprintf( stderr, "_CONDOR_STARTER_PID not set in environment.\n" );
        return -1;
    }
    long starter_pid = strtol( _condor_starter_pid, NULL, 10 );
    if( errno ) {
        fprintf( stderr, "Unable to parse _CONDOR_STARTER_PID '%s'.\n", _condor_starter_pid );
        return -2;
    }

    fprintf( stdout, "\nglobal starter log begins ---\n\n" );
    bool global = printGlobalStarterLog( starter_pid );
    fprintf( stdout, "\n---- global starter log ends; " );

    fprintf( stdout, "local starter log begins ---\n\n" );
    bool local = printLocalStarterLog();
    fprintf( stdout, "\n---- local starter log ends\n\n" );


    return ((global || local) ? 0 : 1);
}
