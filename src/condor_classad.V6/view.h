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
	void GetRankValue( Value &rankValue ) const;

	ViewMember	operator=( const ViewMember& );

private:
	string	key;
	Value	rank;
};


struct ViewMemberLT {
	bool operator()( const ViewMember &vm1, const ViewMember &vm2 ) const;
};


typedef string ViewName;
typedef multiset<ViewMember, ViewMemberLT> ViewMembers;
typedef slist<View*> SubordinateViews;
typedef hash_map<string,View*,StringHash> PartitionedViews;
typedef hash_map<string,ViewMembers::iterator,StringHash> MemberIndex;


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
	void		GetSubordinateViewNames( vector<string>& );
	void		GetPartitionedViewNames( vector<string>& );
	bool		FindPartition( ClassAd *rep, ViewName &partition );

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
	void ClassAdPreModify( ClassAdCollectionServer *coll, ClassAd *ad );
	bool ClassAdModified ( ClassAdCollectionServer *coll, 
			const string &key, ClassAd *ad );
	void ClassAdDeleted  ( ClassAdCollectionServer *coll, 
			const string &key, ClassAd *ad );

		// misc
	bool Display( FILE * );

private:
	friend class ClassAdCollectionServer;
	friend class LocalCollectionQuery;

		// private helper function
	string 	makePartitionSignature( ClassAd *ad );
	void	DeleteView( ClassAdCollectionServer * );

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
