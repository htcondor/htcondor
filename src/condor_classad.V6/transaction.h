#if !defined( XACTION_H )
#define XACTION_H

#include <hash_map>
#include <slist>
#include <string>
#include "collection_ops.h"

class ClassAd;
class ClassAdCollectionServer;
class View;

typedef hash_map<string, ClassAd*>	LocalClassAdTable;
typedef hash_map<string, View*>		LocalViewRegistry;
typedef slist<CollectionLogRecord*>	CollectionOpList;

class Transaction {
public:
	Transaction( );
	~Transaction( );

	inline void SetCollectionServer( ClassAdCollectionServer *c ) { server=c; }
	inline void GetCollectionServer( ClassAdCollectionServer *&c ){ c=server; } 
	inline void SetXactionName( const string &n ) { xactionName = n; }
	inline void GetXactionName( string &n ) const { n = xactionName; }

	ClassAd	*GetClassAd( const string &key );
	bool	GetViewInfo( const string &viewName, ClassAd *&viewInfo );

	bool AppendLog( CollectionLogRecord * );
	bool Log( Sink * );
	bool Check( ClassAdCollectionServer * );
	bool Play( ClassAdCollectionServer * );
	bool Abort( );

private:
	string					xactionName;
	ClassAdCollectionServer	*server;
	CollectionOpList		opList;
	LocalClassAdTable		classadTable;
	LocalViewRegistry		viewRegistry;
};

#endif XACTION_H
