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

private:

		// Private helper methods

	bool getConfigString( ClassAd* ad, const char* key, bool warn,
						  const char* attr, const char* alt_name );
	bool getConfigInt( ClassAd* ad, const char* key, bool warn,
					   const char* attr, const char* alt_name );
	bool getConfigBool( ClassAd* ad, const char* key, bool warn,
						const char* attr, const char* alt_name );
	bool getConfigAttr( ClassAd* ad, const char* key, bool warn,
						bool is_string, const char* attr,
						const char* alt_name );

	bool getUniverse( ClassAd* ad, const char* key );

		// Private data
	char* key;
};


#endif /* _CONDOR_JIC_LOCAL_CONFIG_H */
