#if !defined( VIEW_H )
#define VIEW_H

	// STL includes
#include <string>
#include <set>
#include <hash_map>
#include <slist>

#include "exprTree.h"
#include "matchClassad.h"

BEGIN_NAMESPACE( classad )

	// class declarations
class ExprTree;
class ClassAd;
class View;
class ClassAdCollectionServer;


class ViewMember {
public:
	ViewMember( );
	~ViewMember( );

	void SetKey( const string &key );
	void SetRankValue( const Value &rankValue );

	void GetKey( string &key ) const;
	void GetRankValue( Value &rankValue );

	ViewMember	operator=( const ViewMember& );

private:
	string	key;
	Value		rank;
};


struct hash<const string &> {
	size_t operator()( const string &s ) const { 
		size_t h = 0;
		for( int i = s.size(); i >= 0; i-- ) {
			h = 5*h + s[i];
		}
		return( h );
	}
};

struct ViewMemberLT {
	bool operator()( const ViewMember &vm1, const ViewMember &vm2 ) const;
};


typedef string											ViewName;
typedef multiset<ViewMember, ViewMemberLT> 					ViewMembers;
typedef slist<View*>										SubordinateViews;
typedef hash_map<string,View*,hash<const string &> > PartitionedViews;
typedef hash_map<string,ViewMembers::iterator,hash<const string&> >
	MemberIndex;


/* View class */
class View {
public:
	View( View *parent );
	~View( );

		// view control
	bool SetViewName( const ViewName &viewName );
	bool SetConstraintExpr( ClassAdCollectionServer *coll,
			const string &constraint );
	bool SetConstraintExpr( ClassAdCollectionServer *coll,
			ExprTree *constraint );
	bool SetRankExpr( ClassAdCollectionServer *coll, const string &expr );
	bool SetRankExpr( ClassAdCollectionServer *coll, ExprTree *tree );
	bool SetPartitionExprs( ClassAdCollectionServer *coll, 
			const string &exprList );
	bool SetPartitionExprs( ClassAdCollectionServer *coll, ExprList *el );
	bool SetViewInfo( ClassAdCollectionServer *coll, ClassAd *viewInfo );

		// view interrogation
	inline ViewName GetViewName( ) const { return( viewName ); }
	inline View	*GetParent( ) const { return parent; }
	inline int	Size( ) const { return( viewMembers.size( ) ); }
	ExprTree	*GetConstraintExpr( );
	ExprTree 	*GetRankExpr( );
	ExprList	*GetPartitionAttributes( );
	ClassAd		*GetViewInfo( );
	bool		IsMember( const string &key );

		// child view manipulation
	bool InsertSubordinateView( ClassAdCollectionServer *coll, ClassAd *vInfo );
	bool InsertPartitionedView( ClassAdCollectionServer *coll, ClassAd *vInfo, 
			ClassAd *rep );
	bool DeleteChildView( ClassAdCollectionServer *coll,
			const ViewName &viewName );
	bool DeleteSubordinateView( ClassAdCollectionServer *coll,
			const ViewName &viewName );
	bool DeletePartitionedView( ClassAdCollectionServer *coll,
			const ViewName &viewName );
	bool DeletePartitionedView( ClassAdCollectionServer *coll, ClassAd *rep );
			
		// classad manipulation
	bool ClassAdInserted ( ClassAdCollectionServer *coll, 
			const string &key, ClassAd *ad );
	bool ClassAdPreModify( ClassAdCollectionServer *coll, ClassAd *ad );
	bool ClassAdModified ( ClassAdCollectionServer *coll, 
			const string &key, ClassAd *ad );
	bool ClassAdDeleted  ( ClassAdCollectionServer *coll, 
			const string &key, ClassAd *ad );

	bool Display( FILE * );

private:
	friend class ClassAdCollectionServer;

		// private helper function
	string makePartitionSignature( ClassAd *ad );
	bool		extractPartitionAttrs( vector<string>&, ExprTree * );
	void		DeleteView( ClassAdCollectionServer * );

	ViewName			viewName;			// name of the view
	View				*parent;			// pointer to parent view
	ViewMembers			viewMembers;		// the classads in this view
	MemberIndex			memberIndex;		// keys->ViewMember mapping
	PartitionedViews	partitionedViews;	// views created by partitioning
	SubordinateViews	subordinateViews;	// views explicitly added
	string				oldAdSignature;		// old signature of ad to be changed
	MatchClassAd		evalEnviron;		// also stores view info
};

END_NAMESPACE

#endif // VIEW_H
