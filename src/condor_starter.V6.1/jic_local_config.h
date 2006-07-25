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

#if !defined(_CONDOR_JIC_LOCAL_CONFIG_H)
#define _CONDOR_JIC_LOCAL_CONFIG_H

#include "jic_local.h"

/** 
	This is the child class of JICLocal (and therefore
	JobInfoCommunicator) that deals with running "local" jobs and
	getting the job ClassAd info out of the config file.  The
	constructor takes a string, which is the keyword we prepend to all
	the attributes we want to look up.
*/

class JICLocalConfig : public JICLocal {
public:

		/// Constructor
	JICLocalConfig( const char* keyword, int cluster, int proc, 
					int subproc );

		/// Destructor
	virtual ~JICLocalConfig();

	bool getLocalJobAd( void );

protected:

		/** Protected version of the constructor.  This can't be used
			by the outside world, but it can be used by our derived
			classes.  JICLocalStdin can work in conjunction with a
			keyword, but it's not required, so we don't want to
			EXCEPT() if it's not there.  Other than run-time type
			checking, there's no way for the public constructor to
			know if it should EXCEPT or not, so this seems like the
			safest, most portable way to get what we want.
		*/
	JICLocalConfig( int cluster, int proc, int subproc );

		/** Get the job's universe from the config file.  We can't
			just use the regular getInt() method, since the user could
			have defined either the int or the string version of the
			universe.  We want this to be virtual since JICLocalStdin
			needs a different version (which checks in the existing
			ClassAd before trying the config file).
		*/
	virtual bool getUniverse( void );

		// Protected helper methods
	bool getString( bool warn, const char* attr, const char* alt_name );
	bool getBool( bool warn, const char* attr, const char* alt_name );
	bool getInt( bool warn, const char* attr, const char* alt_name );
	bool getAttr( bool warn, bool is_string, const char* attr,
				  const char* alt_name );

		// Protected data
	char* key;
};


#endif /* _CONDOR_JIC_LOCAL_CONFIG_H */
