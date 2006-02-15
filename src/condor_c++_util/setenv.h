/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
//@}

#endif  // _SETENV_H

