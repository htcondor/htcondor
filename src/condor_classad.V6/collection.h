#if !defined COLLECTION_H
#define COLLECTION_H

#include "view.h"
#include "log.h"
#include "collection_ops.h"

class Transaction;

BEGIN_NAMESPACE( classad )

class ClassAdProxy {
public:
	ClassAdProxy( ){ ad=NULL; };
	~ClassAdProxy( ){ };

	ClassAd	*ad;
};

typedef hash_map<string, View*, hash<const string &> > 
	ViewRegistry;
typedef hash_map<string, ClassAdProxy, hash<const string &> >	
	ClassAdTable;
typedef hash_map<string, Transaction*, hash<const string & > >	
	XactionTable;


class ClassAdCollectionServer {
public:
	ClassAdCollectionServer( const string &filename );
	~ClassAdCollectionServer( );

		// Set rank expr for the root view
	bool SetRootRankExpr( ExprTree *rank );
	bool SetRootRankExpr( const string &rank );

		// View creation/deletion/interrogation
	bool CreateSubView( const string &xactionName, 
			const ViewName &viewName,const ViewName &parentViewName,
			const string &constraint, const string &rank,
			const string &partitionExprs );
	bool CreatePartition( const string &xactionName,
			const ViewName &viewName, const ViewName &parentViewName,
			const string &constraint, const string &rank,
			const string &partitionExprs, ClassAd *rep );
	bool DeleteView( const string &xactionName, const ViewName &viewName );
	bool SetViewInfo( const string &xactionName, const ViewName &viewName,
			const string &constraint, const string &rank,
			const string &partitionAttrs );
	bool GetViewInfo( const ViewName &viewName, ClassAd *&viewInfo );


		// ClassAd manipulation
	bool AddClassAd( const string &xactionName, const string &key, 
			ClassAd *newAd );
	bool UpdateClassAd( const string &xactionName, const string &key, 
			ClassAd *updateAd );
	bool ModifyClassAd( const string &xactionName, const string &key, 
			ClassAd *modifyAd );
	bool RemoveClassAd( const string &xactionName,const string &key );
	ClassAd *GetClassAd(const string &key );


		// Transaction management
	bool OpenTransaction( const string &transactionName );
	bool CloseTransaction( const string &transactionName, bool commit );

		// Query
	// QueryView( viewname, constraint );

		// Trigger registration
	
		// Truncate the log	
	bool TruncateLog( );

		// Misc.
	bool DisplayView( const ViewName &viewName, FILE * );

private:
	friend class View;
	friend class CollectionLogRecord;

	bool ReadLogFile( );
	bool AppendLogRecord( Sink *, CollectionLogRecord * );
	bool LogViews( Sink *snk, View *view, bool subView );
	bool LogState( Sink * );
	bool Check( CollectionLogRecord * );
	bool Play ( CollectionLogRecord * );
	bool ExecuteLogRecord( CollectionLogRecord * );
	CollectionLogRecord *ReadLogEntry( Source * );

		// View registry interface
	bool RegisterView( const ViewName &viewName, View * );
	bool UnregisterView( const ViewName &viewName );

		// Internal tables and associated state
	ViewRegistry	viewRegistry;
	ClassAdTable	classadTable;
	View			viewTree;
	XactionTable	xactionTable;

		// metadata of the log file
	string		logFileName;
	FILE			*log_fp;
	Sink			*sink;
};

END_NAMESPACE

#endif
