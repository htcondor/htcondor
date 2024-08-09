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
 * Return value: std::string - the error to be written to the logs
 * Description : returns the error string, stating that no such parameter is
 *               found in the specified daemon's configuration parameters
 */
std::string
utilNoParameterError( const char* parameter, const char* daemonName );
/* Function    : utilConfigurationParameter
 * Arguments   : parameter - configuration parameter, which caused the error
 *				 daemonName - the name of daemon, the parameter of which 
 *                            caused the error; "HAD" and "REPLICATION" names
 *							  are supported currently
 * Return value: std::string - the error to be written to the logs
 * Description : returns the error string, stating that the parameter is
 *               configured incorrectly in the specified daemon's 
 *				 configuration parameters
 */
std::string
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
/* Function    : utilToSinful
 * Arguments   : address - remote daemon address in either "ip:port" or 
 *				 "hostname:port" format, optionally enclosed in '<>' brackets
 * Return value: char* - allocated by 'malloc' string in canonical '<ip:port>'
 * 				 		 format upon success or NULL upon failure
 * Description : represents the given address in canonical '<ip:port>' format
 */
char* 
utilToSinful( const char* address );

/* Function    : utilSafePutFile
 * Arguments   : socket   - socket, through which the file will be transferred
 *			     filePath - OS path to file, which is to be transferred
 *			     fips_mode- 0 = send leading 16 byte MD5 hash as MAC,
 *                          1 = send 16 leading zeros then a SHA-2 HASH as a hex string
 * Description : transfers the specified file along with its locally calculated
 *				 MAC or HASH
 * Return value: bool - success/failure value
 * Note        : 1) in order to verify that the file is received without errors
 *				 the receiver of it must calculate MAC/HASH on the received file and
 *				 to compare it with the sent MAC/HASH
 *				 2) the socket has to be connected to the receiving side
 */
bool
utilSafePutFile( ReliSock& socket, const std::string& filePath, int fips_mode );

/* Function    : utilSafeGetFile
 * Arguments   : socket   - socket, through which the file will be received
 * 				 filePath - OS path to file, where the received data is to be
 *						    stored
 *				 fips_mode - 0 = check leading MD5 MAC if it is non-zero
 *				             1 = do not check leading MD5 MAC, check SHA-2 HASH if it exists
 * Return value: bool - success/failure value
 * Description : 1) receives the specified file along with its remotely 
 * 				 calculated MAC or hash and verifies that the file was received without 
 *				 errors by calculating the MAC/hash locally and by comparing it to 
 *				 the remote MAC/hash of the file, that has been sent
 *				 2) the socket has to be connected to the transferring side
 */
bool
utilSafeGetFile( ReliSock& socket, const std::string& filePath, int fips_mode );

#endif // UTILS_H
