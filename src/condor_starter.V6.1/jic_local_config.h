/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
