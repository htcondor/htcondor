#include "classad.h"
#include "log.h"

#ifndef COLLECTION_OPS_H
#define COLLECTION_OPS_H

    // collection log operations
enum {
    ClassAdLogOp_NULL               =-1,
    ClassAdLogOp_CreateSubView      = 0,
    ClassAdLogOp_CreatePartition    = 1,
    ClassAdLogOp_DeleteView         = 2,
    ClassAdLogOp_SetViewInfo        = 3,
    ClassAdLogOp_AddClassAd         = 4,
    ClassAdLogOp_UpdateClassAd      = 5,
    ClassAdLogOp_ModifyClassAd      = 6,
    ClassAdLogOp_RemoveClassAd      = 7,
    ClassAdLogOp_OpenTransaction   	= 8,
    ClassAdLogOp_CloseTransaction   = 9,
	ClassAdLogOp_FinishTransaction  = 10
};


class CollectionLogRecord : public LogRecord {
public:
	CollectionLogRecord( );
	~CollectionLogRecord( );

	bool GetStringAttr( const char *, string &val );
	bool SetTransactionName( const string &tname );
	bool SetOpType( int );
	bool Check( void* datastruc );
	bool Play ( void* datastruc );
};


#endif
