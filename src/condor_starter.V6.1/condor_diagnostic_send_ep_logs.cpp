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
        // FIXME:
        // This will miss lines from the FTO if it's a forked child.  Strictly
        // speaking, we should look for our PPID in the first banner line,
        // filter in that line and all others until we see the EXITING WITH
        // STATUS line or a banner line with the wrong PID.
        [=] (const char * line) -> bool { return NULL != strstr(line, pattern.c_str()); }
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

    fprintf( stdout, "global starter log begins ---\n" );
    bool global = printGlobalStarterLog( starter_pid );
    fprintf( stdout, "---- global starter log ends\n" );

    fprintf( stdout, "local starter log begins ---\n" );
    bool local = printLocalStarterLog();
    fprintf( stdout, "---- local starter log ends\n" );


    return ((global && local) ? 0 : 1);
}
