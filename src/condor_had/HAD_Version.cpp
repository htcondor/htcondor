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
// for 'daemonCore'
#include "condor_daemon_core.h"
// for 'StatWrapper'
#include "stat_wrapper.h"
// for 'getHostFromAddr' and 'getPortFromAddr'
#include "internet.h"
#include "condor_ver_info.h"

#include "Version.h"
#include "FilesOperations.h"
#include <fstream>
using namespace std;

time_t Version::m_lastModifiedTime = -1;

static void
createFile(const std::string& filePath)
{
	StatWrapper statWrapper( filePath );
	// if no state file found, create one
	if ( statWrapper.GetRc( ) && statWrapper.GetErrno() == ENOENT) {
		ofstream file( filePath.c_str( ) );
    }
}

Version::Version():
    m_gid( 0 ), m_logicalClock( 0 ), m_state( VERSION_REQUESTING ),
	m_isPrimary( FALSE ), m_knowsNewTransferProtocol( true )
{
}

void
Version::initialize( const std::string& pStateFilePath, 
					 const std::string& pVersionFilePath )
{
	REPLICATION_ASSERT(pStateFilePath != "" && pVersionFilePath != "");
	
    m_lastModifiedTime = -1; 
	m_stateFilePath    = pStateFilePath;
	m_versionFilePath  = pVersionFilePath;
    
    if( ! load( ) ) {
        save( );
    }
    synchronize( false );

	const char *val = daemonCore->InfoCommandSinfulString( );
    m_sinfulString = val ? val : "";
	m_knowsNewTransferProtocol = true;
//char* sinfulStringString = 0;
//    get_full_hostname( hostNameString );
//    hostName = hostNameString;
//    delete [] hostNameString;
}

bool
Version::synchronize(bool isLogicalClockIncremented)
{
	REPLICATION_ASSERT(m_stateFilePath != "" && m_versionFilePath != "");
    dprintf( D_ALWAYS, "Version::synchronize started "
			"(is logical clock incremented = %d)\n",
             int( isLogicalClockIncremented ) );
    load( );        
    createFile( m_stateFilePath );

    StatWrapper statWrapper( m_stateFilePath );
    
	const StatStructType* status      = statWrapper.GetBuf( );
    time_t                currentTime = time( NULL );
    // to contain the time strings produced by 'ctime_r' function, which is
    // reentrant unlike 'ctime' one
	//char                  timeBuffer[BUFSIZ];
	const char *ctime_str;
	ctime_str = ctime( &m_lastModifiedTime );
	std::string lastKnownModifiedTimeString = ctime_str ? ctime_str : "";
	ctime_str = ctime( &status->st_mtime );
	std::string lastModifiedTimeString      = ctime_str ? ctime_str : "";
	ctime_str = ctime( &currentTime );
	std::string currentTimeString           = ctime_str ? ctime_str : "";
    // retrieving access status information
    dprintf( D_FULLDEBUG,
                    "Version::synchronize %s "
                    "before setting last mod. time:\n"
                    "last known mod. time - %sactual mod. time - %s"
                    "current time - %s",
               m_stateFilePath.c_str( ),
               //ctime_r( &m_lastModifiedTime, timeBuffer ),
               //ctime_r( &status->st_mtime, timeBuffer + BUFSIZ / 3 ),
               //ctime_r( &currentTime, timeBuffer + 2 * BUFSIZ / 3 ) );
			   lastKnownModifiedTimeString.c_str( ),
			   lastModifiedTimeString.c_str( ),
			   currentTimeString.c_str( ) );
    // updating the version: by modification time of the underlying file
    // and incrementing the logical version number
    if( m_lastModifiedTime >= status->st_mtime ) {
        return false;
    }
    dprintf( D_FULLDEBUG, "Version::synchronize "
                          "setting version last modified time\n" );
    m_lastModifiedTime = status->st_mtime;
    
    if( isLogicalClockIncremented && m_logicalClock < INT_MAX ) {
		m_logicalClock ++;
        save( );

        return true;
    } else if ( isLogicalClockIncremented /*&& m_logicalClock == INT_MAX*/ ) {
		// to be on a sure side, when the maximal logical clock value is
		// reached, we terminate the replication daemon
		utilCrucialError( "Version::synchronize reached maximal logical clock "
						  "value\n" );
	}

    return false;
}

bool
Version::code( ReliSock& socket )
{
    dprintf( D_ALWAYS, "Version::code started\n" );
    socket.encode( );

   	int isPrimaryAsInteger      = int( m_isPrimary );
   
    if( ! socket.code( m_gid )          /*|| ! socket.end_of_message( )*/ ||
        ! socket.code( m_logicalClock ) /*|| ! socket.end_of_message( )*/ ||
        ! socket.code( m_sinfulString ) /*|| ! socket.end_of_message( )*/ ||
		! socket.code( isPrimaryAsInteger ) ) {
        dprintf( D_NETWORK, "Version::code "
                            "unable to code the version\n");
        return false;
    }
    return true;
}

bool
Version::decode( Stream* stream )
{
    dprintf( D_ALWAYS, "Version::decode started\n" );
    
    int   temporaryGid          = -1;
    int   temporaryLogicalClock = -1;
	std::string temporarySinfulString;
	int  temporaryIsPrimary     = 0;

    stream->decode( );

    if( ! stream->code( temporaryGid ) ) {
        dprintf( D_NETWORK, "Version::decode "
                            "unable to decode the gid\n" );
        return false;
    }

    if( ! stream->code( temporaryLogicalClock ) ) {
        dprintf( D_NETWORK, "Version::decode "
                            "unable to decode the logical clock\n" );
        return false;
    }

    if( ! stream->code( temporarySinfulString ) ) {
        dprintf( D_NETWORK, "Version::decode "
                            "unable to decode the sinful string\n" );
        return false;
    }

	if( ! stream->code( temporaryIsPrimary ) ) {
        dprintf( D_NETWORK, "Version::decode "
                            "unable to decode the 'isPrimary' field\n" );
        return false;
    }

	const CondorVersionInfo *peer_ver = stream->get_peer_version();
	if ( peer_ver ) {
		m_knowsNewTransferProtocol = peer_ver->built_since_version( 8, 7, 4 );
	} else {
		m_knowsNewTransferProtocol = false;
	}

    m_gid          = temporaryGid;
    m_logicalClock = temporaryLogicalClock;
    m_sinfulString = temporarySinfulString;
	m_isPrimary    = temporaryIsPrimary;
    dprintf( D_FULLDEBUG, "Version::decode remote version %s\n", 
			 toString( ).c_str( ) );

    return true;
}

std::string
Version::getHostName( ) const
{
    char*     hostNameString = getHostFromAddr( m_sinfulString.c_str( ) );
    std::string hostName     = hostNameString ? hostNameString : "";

    free( hostNameString );
    dprintf( D_FULLDEBUG, "Version::getHostName returned %s\n", 
			 hostName.c_str( ) );
    return hostName;
}

bool
Version::isComparable( const Version& version ) const
{
    return getGid( ) == version.getGid( ) ;//&&
 // strcmp( getSinfulString( ), version.getSinfulString( ) ) == 0;
}

bool
Version::operator > ( const Version& version ) const
{
    dprintf( D_FULLDEBUG, "Version::operator > comparing %s vs. %s\n",
               toString( ).c_str( ), version.toString( ).c_str( ) );
    
    if( getState( ) == REPLICATION_LEADER &&
        version.getState( ) != REPLICATION_LEADER ) {
        return true;
    }
    if( getState( ) != REPLICATION_LEADER &&
        version.getState( ) == REPLICATION_LEADER ) {
        return false;
    }
    return getLogicalClock( ) > version.getLogicalClock( );
}

bool
Version::operator >= (const Version& version) const
{
    dprintf( D_FULLDEBUG, "Version::operator >= started\n" );
    return ! ( version > *this);
}

std::string
Version::toString( ) const
{
	std::string versionAsString;

	formatstr( versionAsString,
		"logicalClock = %d, gid = %d, belongs to %s, transferProtocol = %s",
		m_logicalClock, m_gid, m_sinfulString.c_str(),
		m_knowsNewTransferProtocol ? "new" : "old" );

	return versionAsString;
}
/* Function    : load
 * Return value: bool - success/failure value
 * Description : loads Version components from the underlying OS file
 *				 to the appropriate object data members
 * Note        : the function is like public 'load' with one only difference - 
 *				 it changes the state of the object itself
 */
bool
Version::load( )
{
    dprintf( D_ALWAYS, "Version::load of %s started\n", 
			 m_versionFilePath.c_str( ) );
//    char     buffer[BUFSIZ];
//    ifstream versionFile( m_versionFilePath.c_str( ) );
//
//    if( ! versionFile.is_open( ) ) {
//        dprintf( D_FAILURE, "Version::load unable to open %s\n",
//                 m_versionFilePath.c_str( ) );
//        return false;
//    }
    // read gid
//    if( versionFile.eof( ) ) {
//        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
//                            "nothing appears inside it\n", 
//				 m_versionFilePath.c_str( ) );
//        return false;
//    }
//    versionFile.getline( buffer, BUFSIZ );
//
//    int temporaryGid = atol( buffer );
//
//    dprintf( D_FULLDEBUG, "Version::load gid = %d\n", temporaryGid );
    // read version
//    if( versionFile.eof( ) ) {
//        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
//                			"only gid appears inside it\n", 
//				 m_versionFilePath.c_str( ) );
//        return false;
//    }
//
//    versionFile.getline( buffer, BUFSIZ );
//    int temporaryLogicalClock = atol( buffer );
//
//    dprintf( D_FULLDEBUG, "Version::load version = %d\n", 
//			 temporaryLogicalClock );
	int temporaryGid = -1, temporaryLogicalClock = -1;
    
	if( ! load( temporaryGid, temporaryLogicalClock ) ) {
		return false;
	}
	
	m_gid          = temporaryGid;
    m_logicalClock = temporaryLogicalClock;

    return true;
}

bool
Version::load( int& temporaryGid, int& temporaryLogicalClock ) const
{
    char     buffer[BUFSIZ];
    ifstream versionFile( m_versionFilePath.c_str( ) );

    if( ! versionFile.is_open( ) ) {
        dprintf( D_FAILURE, "Version::load unable to open %s\n",
                 m_versionFilePath.c_str( ) );
        return false;
    }
    // read gid
    if( versionFile.eof( ) ) {
        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
                            "nothing appears inside it\n",
                 m_versionFilePath.c_str( ) );
        return false;
    }
    versionFile.getline( buffer, BUFSIZ );

    temporaryGid = atol( buffer );

    dprintf( D_FULLDEBUG, "Version::load gid = %d\n", temporaryGid );
    // read version
    if( versionFile.eof( ) ) {
        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
                            "only gid appears inside it\n",
                 m_versionFilePath.c_str( ) );
        return false;
    }
    versionFile.getline( buffer, BUFSIZ );
    temporaryLogicalClock = atol( buffer );

    dprintf( D_FULLDEBUG, "Version::load version = %d\n",
             temporaryLogicalClock );
    REPLICATION_ASSERT(temporaryGid >= 0 && temporaryLogicalClock >= 0);

	return true;
}
/* Function   : save
 * Description: saves the Version object components to the underlying OS file
 */
void
Version::save( )
{
    dprintf( D_ALWAYS, "Version::save started\n" );

    ofstream versionFile( m_versionFilePath.c_str( ) );

    versionFile << m_gid << endl << m_logicalClock;
    //versionFile.close( );
    // finding the new last modification time
//    StatWrapper statWrapper( m_stateFilePath );
//
//    if ( statWrapper.GetRc( ) ) {
//        EXCEPT("Version::synchronize cannot get %s status "
//               "due to errno = %d", 
//               m_versionFilePath.c_str( ), 
//				 statWrapper.GetErrno());
//    }
//    m_lastModifiedTime = statWrapper.GetBuf( )->st_mtime;
}
