/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

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

    ClassAdCollectionServer(const ClassAdCollectionServer &collection)           { return; }
    ClassAdCollectionServer operator=(const ClassAdCollectionServer &collection) { return *this; }

	// remote interrogation service function
	bool HandleReadOnlyCommands( int, ClassAd *, Sock * );
	bool HandleQuery( ClassAd *, Sock * );

};

END_NAMESPACE

#endif
