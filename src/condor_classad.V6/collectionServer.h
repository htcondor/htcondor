/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

#if !defined( COLLECTION_SERVER_H )
#define COLLECTION_SERVER_H

#include "collection.h"
#include "collectionBase.h"
#include "condor_io.h"


BEGIN_NAMESPACE( classad )

class ClassAdCollectionServer : public ClassAdCollection {
public:
	ClassAdCollectionServer( );
	virtual ~ClassAdCollectionServer( );

	int HandleClientRequest( int command, Sock *clientSock );



protected:
	virtual bool OperateInNetworkMode( int, ClassAd *, Sock * );

private:
	friend class View;
	friend class ServerTransaction;
	friend class ClassAd;
	friend class LocalCollectionQuery;

	// remote interrogation service function
	bool HandleReadOnlyCommands( int, ClassAd *, Sock * );
	bool HandleQuery( ClassAd *, Sock * );

};

END_NAMESPACE

#endif
