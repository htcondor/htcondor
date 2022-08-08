#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "classad/classad.h"
#include "compat_classad.h"
#include "ToE.h"
#include "iso_dates.h"
#include "utc_time.h"
#include "stl_string_utils.h"

namespace ToE {

const int OfItsOwnAccord = 0;
const int DeactivateClaim = 1;
const int DeactivateClaimForcibly = 2;

const char * strings[] = {
    "OF_ITS_OWN_ACCORD",
    "DEACTIVATE_CLAIM",
    "DEACTIVATE_CLAIM_FORCIBLY"
};

const char * itself = "itself";

// Append the toe ad to the .job.ad file.  This avoids a race
// condition where the startd replaces the .update.ad with a
// new after the ToE tag is written but before the post script
// gets a chance to look at it.  Using append mode means that
// we don't need locking to ensure the job ad file remains
// syntactically valid.  (The last writer will win, but the
// only case where that could be wrong is if the starter sees
// the job terminate of its own accord but the startd is
// ordered to terminate the job between when the starter writes
// this file and when the post-script runs.)
//
// On Windows, the C runtime library emulates append mode, which
// means it won't work for the purpose we're using it for.  We
// should probably wrap this knowledge up in a function somewhere
// if we aren't going to fixe safefile.
bool
writeTag( classad::ClassAd * toe, const std::string & jobAdFileName ) {
#ifndef WINDOWS
    FILE * jobAdFile = safe_fopen_wrapper_follow( jobAdFileName.c_str(), "a" );
#else
    FILE * jobAdFile = NULL;
    HANDLE hf = CreateFile( jobAdFileName.c_str(),
        FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( hf == INVALID_HANDLE_VALUE ) {
        dprintf( D_ALWAYS, "Failed to open .job.ad file: %d\n", GetLastError() );
        jobAdFile = NULL;
    } else {
        int fd = _open_osfhandle((intptr_t)hf, O_APPEND);
        if( fd == -1 ) {
            dprintf( D_ALWAYS, "Failed to _open .job.ad file: %d\n", _doserrno );
        }
        jobAdFile = fdopen( fd, "a" );
    }
#endif

    if(! jobAdFile) {
        dprintf( D_ALWAYS, "Failed to write ToE tag to .job.ad file (%d): %s\n",
            errno, strerror(errno) );
        return false;
    } else {
        fPrintAd( jobAdFile, * toe );
        fclose( jobAdFile );
    }

    return true;
}

bool
decode( classad::ClassAd * ca, Tag & tag ) {
    if(! ca) { return false; }

    ca->EvaluateAttrString( "Who", tag.who );
    ca->EvaluateAttrString( "How", tag.how );
    long long when;
    ca->EvaluateAttrNumber( "When", when );
    ca->EvaluateAttrNumber( "HowCode", (int &)tag.howCode );

    if( ca->EvaluateAttrBool( ATTR_ON_EXIT_BY_SIGNAL, tag.exitBySignal )) {
        ca->EvaluateAttrNumber(
            tag.exitBySignal ? ATTR_ON_EXIT_SIGNAL : ATTR_ON_EXIT_CODE,
            tag.signalOrExitCode
        );
    }

    char whenStr[ISO8601_DateAndTimeBufferMax];
    struct tm eventTime;
    // time_t is a signed long int on all Linux platforms.  We send
    // it over the wire via ClassAds in a (signed) long long, which
    // is 64 bits on all platforms.  This conversion should be a no
    // op on 64-bit platforms, but will correctly preserve the sign
    // bit on 32-bit platforms.
    time_t ttWhen = (time_t)when;
    gmtime_r( & ttWhen, & eventTime );
    // We should also respect the time format flag, once local times
    // round-trip between differente machines (e.g., once we write
    // time zones as part of our time stamps).
    time_to_iso8601( whenStr, eventTime, ISO8601_ExtendedFormat,
        ISO8601_DateAndTime, true );
    tag.when.assign( whenStr );

    return true;
}

bool
Tag::writeToString( std::string & out ) const {
    return formatstr_cat( out,
        "\n\tJob terminated by %s at %s (using method %d: %s).\n",
        who.c_str(), when.c_str(), howCode, how.c_str() ) >= 0;
}

bool
Tag::readFromString( const std::string & in ) {
	size_t startPos = 0;

	const char * endWhoStr = " at ";
	size_t endWho = in.find( endWhoStr );
	if( endWho == std::string::npos ) { return false; }
	this->who = in.substr( startPos, endWho-startPos );
	startPos = endWho + strlen(endWhoStr);

	const char * endWhenStr = " (using method ";
	size_t endWhen = in.find( endWhenStr, startPos );
	if( endWhen == std::string::npos ) { return false; }
	std::string whenStr = in.substr( startPos, endWhen-startPos );
	startPos = endWhen + strlen(endWhenStr);
	// This code gets more complicated if we don't assume UTC i/o.
	struct tm eventTime;
	iso8601_to_time( whenStr.c_str(), & eventTime, NULL, NULL );
	formatstr( when, "%ld", timegm(&eventTime) );

	const char * endHowCodeStr = ": ";
	size_t endHowCode = in.find( endHowCodeStr, startPos );
	if( endHowCode == std::string::npos ) { return false; }
	std::string howCodeStr = in.substr( startPos, endHowCode-startPos );
	startPos = endHowCode + strlen(endHowCodeStr);
	char * end = NULL;
	long lhc = strtol( howCodeStr.c_str(), & end, 10 );
	if( end && *end == '\0' ) {
		this->howCode = lhc;
	} else {
		return false;
	}

	const char * endHowStr = ").";
	size_t endHow = in.find( endHowStr, startPos );
	if( endHow == std::string::npos ) { return false; }
	this->how = in.substr( startPos, endHow-startPos );
	startPos = endHow + strlen(endHowStr);
	if(startPos < in.length()) { return false; }

    return true;
}

bool
encode( const Tag & tag, classad::ClassAd * ca ) {
    if(! ca) { return false; }

    ca->InsertAttr( "Who", tag.who );
    ca->InsertAttr( "How", tag.how );
    ca->InsertAttr( "When",tag.when );
    ca->InsertAttr( "HowCode", (int)tag.howCode );

    if( tag.howCode == ToE::OfItsOwnAccord ) {
        ca->InsertAttr( ATTR_ON_EXIT_BY_SIGNAL, tag.exitBySignal );
        ca->InsertAttr(
            tag.exitBySignal ? ATTR_ON_EXIT_SIGNAL : ATTR_ON_EXIT_CODE,
            tag.signalOrExitCode
        );
    }

    return true;
}

};
