/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
 
/*
  This file holds utility functions that rely on ClassAds.
*/


/*
  lookup ATTR_KILL_SIG, but if it's a string represenation, convert it
  to the appropriate signal number for the local platform.
*/
int findSoftKillSig( ClassAd* ad );

// same as findSoftKillSig(), but for ATTR_REMOVE_KILL_SIG
int findRmKillSig( ClassAd* ad );

// same as findSoftKillSig(), but for ATTR_HOLD_KILL_SIG
int findHoldKillSig( ClassAd* ad );

// Based on info in the ClassAd and the given exit reason, construct
// the appropriate string describing the fate of the job...
bool printExitString( ClassAd* ad, int exit_reason, MyString &str );

// Create an empty job ad, with sensible defaults for all of the attributes
// that the schedd expects to be set, like condor_submit would set them.
// owner, universe, and cmd are the only attributes that require an
// explicit value. If NULL is passed for owner, the attribute is explicitly
// set to Undefined, which tells the schedd to fill in the attribute. This
// feature is only used by the soap interface currently.
// The caller is responible for calling 'delete' on the returned ClassAd.
ClassAd *CreateJobAd( const char *owner, int universe, const char *cmd );
