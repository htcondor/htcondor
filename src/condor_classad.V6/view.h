/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __VIEW_H__
#define __VIEW_H__

// STL includes
#include <string>
#include <set>
#if (__GNUC__<3)
#include <hash_map>
#include <slist>
#else
#include <ext/hash_map>
#include <ext/slist>
using namespace __gnu_cxx;
#endif
#include "exprTree.h"
#include "matchClassad.h"

BEGIN_NAMESPACE( classad );

	// class declarations
class ExprTree;
class ClassAd;
class View;
class ClassAdCollection;
class ClassAdCollectionServer;


class ViewMember {
public:
	ViewMember( );
	ViewMember(const ViewMember &other);
	~ViewMember( );

	void SetKey( const string &key );
	void SetRankValue( const Value &rankValue );

	void GetKey( string &key ) const;
	void GetRankValue( Value &rankValue ) const;

	ViewMember	operator=( const ViewMember& );
	friend bool operator<(const ViewMember &vm1, const ViewMember &vm2);
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
	bool SetConstraintExpr( ClassAdCollection *coll,
			const string &constraint );
	bool SetConstraintExpr( ClassAdCollection *coll,
			ExprTree *constraint );
	bool SetRankExpr( ClassAdCollection *coll, const string &expr );
	bool SetRankExpr( ClassAdCollection *coll, ExprTree *tree );
	bool SetPartitionExprs( ClassAdCollection *coll, 
			const string &exprList );
	bool SetPartitionExprs( ClassAdCollection *coll, ExprList *el );
	bool SetViewInfo( ClassAdCollection *coll, ClassAd *viewInfo );

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
	bool InsertSubordinateView( ClassAdCollection *coll, ClassAd *vInfo );
	bool InsertPartitionedView( ClassAdCollection *coll, ClassAd *vInfo, 
			ClassAd *rep );
	bool DeleteChildView( ClassAdCollection *coll,
			const ViewName &viewName );
	bool DeleteSubordinateView( ClassAdCollection *coll,
			const ViewName &viewName );
	bool DeletePartitionedView( ClassAdCollection *coll,
			const ViewName &viewName );
	bool DeletePartitionedView( ClassAdCollection *coll, ClassAd *rep );
			
		// classad manipulation
	bool ClassAdInserted ( ClassAdCollection *coll, 
			const string &key, ClassAd *ad );
	void ClassAdPreModify( ClassAdCollection *coll, ClassAd *ad );
	bool ClassAdModified ( ClassAdCollection *coll, 
			const string &key, ClassAd *ad );
	void ClassAdDeleted  ( ClassAdCollection *coll, 
			const string &key, ClassAd *ad );

		// misc
	bool Display( FILE * );

private:
	friend class ClassAdCollection;
	friend class ClassAdCollectionServer;
	friend class LocalCollectionQuery;

		// private helper function
	string 	makePartitionSignature( ClassAd *ad );
	void	DeleteView( ClassAdCollection * );

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
