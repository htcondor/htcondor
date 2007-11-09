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


#if !defined( COLLECTION_SERVER_H )
#define COLLECTION_SERVER_H

#include "classad/collection.h"
#include "classad/collectionBase.h"
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
