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

#ifndef _SETENV_H
#define _SETENV_H

/** @name Environment management.
 */
//@{
/** Put the {key, value} pair into the environment
    @param key
    @param value
    @return TRUE on success, FALSE on failure
*/
int SetEnv( const char *key, const char *value);

/** Put env_var into the environment
    @param env_var Desired string to go into environment;
    must be of the form 'name=value' 
    @return TRUE on success, FALSE on failure
*/
int SetEnv( const char *env_var );

/** Remove env_var from the environment
    @param env_var Desired variable name to remove from environment;
    @return TRUE on success, FALSE on failure
*/
int UnsetEnv( const char *env_var );

/** Look up env_var in the environment
    @param env_var Desired variable name to search for in the environment;
    @return A pointer to the corresponding value or NULL (if an error
	occurred or the variable is not present). The returned string should
	not be deallocated and is valid until the next call to GenEnv() or
	SetEnv().
*/
const char *GetEnv( const char *env_var );

/** Look up env_var in the environment
    @param env_var Desired variable name to search for in the environment;
	@param result A buffer in which to place the result.
    @return A pointer to the corresponding value or NULL (if an error
	occurred or the variable is not present). The returned string should
	not be deallocated.  It is merely a pointer to result.c_str().
*/
const char *GetEnv( const char *env_var, std::string &result );

char **GetEnviron();
//@}

#endif  // _SETENV_H

