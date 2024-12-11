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
#if defined( WINDOWS_HAS_POSIX_GETLINE )
    FILE * file = safe_fopen_no_create_follow( path.string().c_str(), "r" );
    if( file == NULL ) {
        fprintf( stderr, "Failed to open %s\n", path.string().c_str() );
        return false;
    }

    char * line = NULL;
    size_t n = 0;

    while( getline( & line, & n, file ) != -1 ) {
        size_t size = strlen(line);
        if( size == 0 ) { continue; }
        if( line[size - 1] == '\n' ) { line[size - 1] = '\0'; }
        if( filter( line ) ) {
            fprintf( stdout, "%s\n", line );
        }
    }

    if( line != NULL ) {
        free( line );
    }

    return true;
#else
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
#endif
}


bool
printGlobalStarterLog() {
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
    formatstr( pattern, "(pid:%d)", getppid() );
    std::filesystem::path logPath(LOG);
    return dumpFileToStandardOut(
        logPath / ("StarterLog." + NAME),
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


    bool global = printGlobalStarterLog();
    bool local = printLocalStarterLog();


    return ((global && local) ? 0 : 1);
}
