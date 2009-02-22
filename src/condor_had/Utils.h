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

#ifndef UTILS_H
#define UTILS_H

#include "condor_daemon_core.h"
#include "MyString.h"

#define DEFAULT_SEND_COMMAND_TIMEOUT                             (5)
#define DEFAULT_MAX_TRANSFER_LIFETIME                           (300)
#define IO_RETRY_TIMES                                           (3)

//#define MAX_FILE_SIZE                                      (1000000)
// the lifetime is measured in units of HAD_REPLICATION_INTERVAL
#define VERSION_FILE_NAME                                        "Version"
#define UPLOADING_TEMPORARY_FILES_EXTENSION                      "up"
#define DOWNLOADING_TEMPORARY_FILES_EXTENSION                    "down"

#define REPLICATION_ASSERT(expression)   if( ! ( expression ) ) {            \
                                         	utilCrucialError(#expression );  \
                                		}
/* Function    : utilNoParameterError
 * Arguments   : parameter - configuration parameter, which caused the error
 *               daemonName - the name of daemon, the parameter of which
 *                            caused the error; "HAD" and "REPLICATION" names
 *                            are supported currently
 * Return value: MyString - the error to be written to the logs
 * Description : returns the error string, stating that no such parameter is
 *               found in the specified daemon's configuration parameters
 */
MyString
utilNoParameterError( const char* parameter, const char* daemonName );
/* Function    : utilConfigurationParameter
 * Arguments   : parameter - configuration parameter, which caused the error
 *				 daemonName - the name of daemon, the parameter of which 
 *                            caused the error; "HAD" and "REPLICATION" names
 *							  are supported currently
 * Return value: MyString - the error to be written to the logs
 * Description : returns the error string, stating that the parameter is
 *               configured incorrectly in the specified daemon's 
 *				 configuration parameters
 */
MyString
utilConfigurationError( const char* parameter, const char* daemonName );
/* Function    : utilCrucialError 
 * Arguments   : message - message to be printed before the daemon aborts 
 * Description : prints the given message to logs and aborts
 */
void
utilCrucialError( const char* message );
/* Function    : utilCancelTimer 
 * Arguments   : timerId - reference to daemon timer to be nullified 
 * Description : cancels and nullifies the timer
 */
void 
utilCancelTimer(int& timerId);
/* Function    : utilCancelReaper
 * Arguments   : reaperId - reference to daemon reaper to be nullified
 * Description : cancels and nullifies the reaper
 */
void 
utilCancelReaper(int& reaperId);
/* Function    : utilToString 
 * Arguments   : command - id 
 * Return value: const char* - static string representation of the given command
 * Description : represents the given command id in string format
 */
const char* 
utilToString( int command );
/* Function    : utilToSinful
 * Arguments   : address - remote daemon address in either "ip:port" or 
 *				 "hostname:port" format, optionally enclosed in '<>' brackets
 * Return value: char* - allocated by 'malloc' string in canonical '<ip:port>'
 * 				 		 format upon success or NULL upon failure
 * Description : represents the given address in canonical '<ip:port>' format
 */
char* 
utilToSinful( char* address );
/* Function    :  utilAtoi
 * Arguments   :  string - string to convert
 *				  result - boolean indicator whether the conversion succeded
 * Return value:  int - the value of conversion upon success and 
 *						0 upon failure
 * Description : converts the specified string to integer
 * Note        : the returned value is valid only if 'result' is equal to true
 *				 after the function is called
 */
int 
utilAtoi(const char* string, bool* result);
/* Function   : utilClearList
 * Arguments  : list - string list to be cleared
 * Description: clears list of strings
 */
void
utilClearList( StringList& list );
/* Function    : utilSafePutFile
 * Arguments   : socket   - socket, through which the file will be transferred
 *			     filePath - OS path to file, which is to be transferred
 * Description : transfers the specified file along with its locally calculated
 *				 MAC
 * Return value: bool - success/failure value
 * Note        : 1) in order to verify that the file is received without errors
 *				 the receiver of it must calculate MAC on the received file and
 *				 to compare it with the sent MAC
 *				 2) the socket has to be connected to the receiving side
 */
bool
utilSafePutFile( ReliSock& socket, const MyString& filePath );
/* Function    : utilSafeGetFile
 * Arguments   : socket   - socket, through which the file will be received
 * 				 filePath - OS path to file, where the received data is to be
 *						    stored
 * Return value: bool - success/failure value
 * Description : 1) receives the specified file along with its remotely 
 * 				 calculated MAC and verifies that the file was received without 
 *				 errors by calculating the MAC locally and by comparing it to 
 *				 the remote MAC of the file, that has been sent
 *				 2) the socket has to be connected to the transferring side
 */
bool
utilSafeGetFile( ReliSock& socket, const MyString& filePath );

/* Function   : utilClearList
 * Arguments  : list - the list to be cleared
 * Description: function to clear generic lists
 */
template <class T>
void 
utilClearList( List<T>& list )
{
    T* element = NULL;
    list.Rewind ();

    while ( list.Next( element ) ) {
        delete element;
        list.DeleteCurrent( );
    }
}
/* Function   : utilCopyList
 * Arguments  : lhs - the list to be assigned to
 *              rhs - the list to be copied
 * Description: function to copy generic lists
 */
template <class T>
void 
utilCopyList(List<T>& lhs, List<T>& rhs)
{
    T* element = NULL;
    rhs.Rewind( );

    while( ( rhs.Next( element ) ) )
    {
        dprintf( D_FULLDEBUG, "utilCopyList: %s ", 
				 element->toString().Value() );
        lhs.Append( element );
    }

    lhs.Rewind( );
    rhs.Rewind( );
}

#endif // UTILS_H
