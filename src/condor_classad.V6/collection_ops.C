#include "condor_common.h"
#include "collection.h"
#include "collection_ops.h"

CollectionLogRecord::
CollectionLogRecord( )
{
}


CollectionLogRecord::
~CollectionLogRecord( )
{
}


bool CollectionLogRecord::
SetTransactionName( const string &tname )
{
	return( InsertAttr( "XactionName", tname.c_str( ) ) );
}

bool CollectionLogRecord::
SetOpType( int opType )
{
	return( InsertAttr( "OpType", opType ) );
}

bool CollectionLogRecord::
Check( void* datastruc )
{
	ClassAdCollectionServer *coll = (ClassAdCollectionServer*) datastruc;
	return( coll->Check( this ) );
}

bool CollectionLogRecord::
Play( void* datastruc )
{
	ClassAdCollectionServer *coll = (ClassAdCollectionServer*) datastruc;
	return( coll->Play( this ) );
}

bool CollectionLogRecord::
GetStringAttr( const char *attrName, string &attrValue )
{
	char	*str=NULL;

	if( !EvaluateAttrString( attrName, str ) || !str ) return( false );
	attrValue = str;
	return( true );
}
