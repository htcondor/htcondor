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

#ifndef VERSION_H
#define VERSION_H

// for 'ReplicatorState'
#include "ReplicatorState.h"
#include "Utils.h"
#include "reli_sock.h"

/* Class      : Version
 * Description: class, representing a version of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class Version
{
public:
	// inner class true/false values
    // typedef enum SuccessValue { VERSION_FALSE = 0, VERSION_TRUE };
    /* Function: Version constructor
     */
	Version();
// Operations
	/* Function   : initialize
	 * Arguments  : pStateFilePath - OS path to state file
	 *  			pVersionFilePath - OS path to version file
	 * Description: initializes all data members
	 */
    void initialize( const std::string& pStateFilePath, 
					 const std::string& pVersionFilePath );
	/* Function    : synchronize
     * Arguments   : isLogicalClockIncremented - whether to increment the 
	 *				 logical clock or not
	 * Return value: true - if the state file was modified since the last known
	 *				 modification time and 'isLogicalClockIncremented' is true;
	 *				 false - otherwise
	 * Description : synchronizes local state file version according to the OS
	 *				 state file; if it has been updated and the last 
	 *				 modification time of it is later than the recorded one, 
	 *				 then the Version object is updated, i.e. the OS file is 
	 *				 opened, its fields are loaded into the data members and its
	 *				 last modification time is assigned to 'm_lastModifiedTime'
     */
    bool synchronize(bool isLogicalClockIncremented = true);
	/* Function   : code
	 * Arguments  : socket - socket, through which the date is written
     * Description: write the inner Version object representation to the socket
     */
    bool code(ReliSock& );
	/* Function   : decode
     * Arguments  : stream - socket, through which the date is received
     * Description: receive remote Version object representation from the socket
     */
    bool decode(Stream* );
// End of operations
// Inspectors    
	/* Function    : getGid
     * Return value: int - gid
     * Description : returns gid
     */
    int         getGid()          const { return m_gid; };
    /* Function    : getLogicalClock
     * Return value: int - logical clock
     * Description : returns logical clock
     */
	int         getLogicalClock() const { return m_logicalClock; };
	std::string getSinfulString() const { return m_sinfulString; };
	/* Function    : getHostName
	 * Return value: std::string - this replication daemon host name
	 * Description : returns this replication daemon host name
	 */
	std::string    getHostName()     const;
    /* Function    : load
 	 * Arguments   : temporaryGid - the value of OS file gid field will be 
	 *								assigned to the parameter
 	 *               temporaryLogicalClock - the value of OS file logical clock
 	 *                                       field will be assigned to the 
	 *										 parameter
 	 * Return value: bool - success/failure value
 	 * Description : loads Version components from the underlying OS file to
 	 *               to the specified arguments
 	 */
	bool load( int& temporaryGid, int& temporaryLogicalClock ) const;
	bool knowsNewTransferProtocol() const { return m_knowsNewTransferProtocol; };
// End of inspectors
// Comparison operators
	/* Function    : isComparable
	 * Arguments   : version - the compared version
	 * Return value: bool - true/false value
     * Description : the versions are comparable, if their gids are identical
     */
    bool isComparable(const Version& version) const;
	/* Function    : operator >
     * Arguments   : version - the compared version
     * Return value: bool - true/false value
     * Description : the version is bigger than another, if its logical clock is
	 *				 bigger or if the state of the local daemon is 
	 *				 REPLICATION_LEADER, whilst the state of the remote daemon
	 *				 is not
	 * Note        : the comparison is used, while choosing the best version in
	 *				 VERSION_DOWNLOADING state, to simply compare the logical
	 *				 clocks the versions' states must be set to BACKUP
     */
    bool operator > (const Version& version) const;
	/* Function    : operator >=
     * Arguments   : version - the compared version
     * Return value: bool - true/false value
     * Description : the version is bigger/equal than another, if its logical 
	 * 				 clock is bigger/equal or if the state of the local daemon
	 *				 is REPLICATION_LEADER, whilst the state of the remote
	 *				 daemon is not
     */
    bool operator >= (const Version& version) const;
//    friend bool operator == (const Version& , const Version& );
// End of comparison operators
// Mutators
	/* Function   : setState
	 * Arguments  : newState - new state of the replication daemon to send to
	 *				the newly joined machine
     * Description: sets the state of the replication daemon to send to the
	 *				newly joined machine
     */
    void setState(const ReplicatorState& newState) { m_state = newState; };
	/* Function   : setState
     * Arguments  : version - the version, the state of which is assigned to
     *                        the current version's one
     * Description: sets the state of the replication daemon to send to the
     *              newly joined machine as the specified version's one
     */
    void setState(const Version& version) { m_state = version.getState(); };
	/* Function   : setGid
     * Arguments  : newGid - new gid of the version
     * Description: sets the gid of the version
     */
	void setGid(int newGid) { m_gid = newGid; save( ); };
// End of mutators
// Convertors
	/* Function    : toString
     * Return value: std::string - string representation of Version object
	 * Description : represents the Version object as string
     */
    std::string toString() const;
// End of convertors

private:
	/* Function    : getState
     * Return value: ReplicatorState - this replication daemon current state
     * Description : returns this replication daemon current state
     */   
    const ReplicatorState& getState() const { return m_state; };
    bool load( );
    void save( );
	// static data members
    static time_t         m_lastModifiedTime;
   
	// configuration variables
	std::string			  m_stateFilePath;
	std::string			  m_versionFilePath;
 
	// components of the version
    int                   m_gid;
    int                   m_logicalClock;
    std::string           m_sinfulString;
	ReplicatorState       m_state;
	// added support for conservative policy of accepting updates from primary
	// HAD machines only
	int					  m_isPrimary;
	bool                  m_knowsNewTransferProtocol;
};
//bool operator == (const Version& , const Version& );

#endif // VERSION_H
