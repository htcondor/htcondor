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

#include "classad_stl.h"
#include "exprTree.h"
#include "matchClassad.h"

BEGIN_NAMESPACE( classad )

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

	void SetKey( const std::string &key );
	void SetRankValue( const Value &rankValue );

	void GetKey( std::string &key ) const;
	void GetRankValue( Value &rankValue ) const;

	ViewMember	operator=( const ViewMember& );
	friend bool operator<(const ViewMember &vm1, const ViewMember &vm2);
private:
	std::string	key;
	Value	rank;
};


struct ViewMemberLT {
	bool operator()( const ViewMember &vm1, const ViewMember &vm2 ) const;
};


typedef std::string ViewName;
typedef std::multiset<ViewMember, ViewMemberLT> ViewMembers;
typedef classad_slist<View*> SubordinateViews;
typedef classad_hash_map<std::string,View*,StringHash> PartitionedViews;
typedef classad_hash_map<std::string,ViewMembers::iterator,StringHash> MemberIndex;


/* View class */
class View {
public:
	View( View *parent );
	~View( );

		// view control
	bool SetViewName( const ViewName &viewName );
	bool SetConstraintExpr( ClassAdCollection *coll,
			const std::string &constraint );
	bool SetConstraintExpr( ClassAdCollection *coll,
			ExprTree *constraint );
	bool SetRankExpr( ClassAdCollection *coll, const std::string &expr );
	bool SetRankExpr( ClassAdCollection *coll, ExprTree *tree );
	bool SetPartitionExprs( ClassAdCollection *coll, 
			const std::string &exprList );
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
	bool		IsMember( const std::string &key );
	void		GetSubordinateViewNames( std::vector<std::string>& );
	void		GetPartitionedViewNames( std::vector<std::string>& );
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
			const std::string &key, ClassAd *ad );
	void ClassAdPreModify( ClassAdCollection *coll, ClassAd *ad );
	bool ClassAdModified ( ClassAdCollection *coll, 
			const std::string &key, ClassAd *ad );
	void ClassAdDeleted  ( ClassAdCollection *coll, 
			const std::string &key, ClassAd *ad );

		// misc
	bool Display( FILE * );

	/** An iterator we can use on a View */
	typedef ViewMembers::iterator iterator;

	/** A constatnt iterator we can use on a View */
	typedef ViewMembers::const_iterator const_iterator;

	/** Returns an iterator pointing to the beginning of the
		attribute/value pairs in the View */
	iterator begin() { return viewMembers.begin(); }

	/** Returns a constant iterator pointing to the beginning of the
		attribute/value pairs in the View */
	const_iterator begin() const { return viewMembers.begin(); }
	
	/** Returns aniterator pointing past the end of the
		attribute/value pairs in the View */
	iterator end() { return viewMembers.end(); }
	
	/** Returns a constant iterator pointing past the end of the
		attribute/value pairs in the View */
	const_iterator end() const { return viewMembers.end(); }

private:
	friend class ClassAdCollection;
	friend class ClassAdCollectionServer;
	friend class LocalCollectionQuery;

		// private helper function
	std::string makePartitionSignature( ClassAd *ad );
	void        DeleteView( ClassAdCollection * );

	ViewName			viewName;			// name of the view
	View				*parent;			// pointer to parent view
	ViewMembers			viewMembers;		// the classads in this view
	MemberIndex			memberIndex;		// keys->ViewMember mapping
	PartitionedViews	partitionedViews;	// views created by partitioning
	SubordinateViews	subordinateViews;	// views explicitly added
	std::string			oldAdSignature;		// old signature of ad to be changed
	MatchClassAd		evalEnviron;		// also stores view info
};

END_NAMESPACE

#endif // VIEW_H
