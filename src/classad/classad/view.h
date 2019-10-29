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


#ifndef __CLASSAD_VIEW_H__
#define __CLASSAD_VIEW_H__

// STL includes
#include <string>
#include <set>

#include "classad/classad_containers.h"
#include "classad/exprTree.h"
#include "classad/matchClassad.h"

namespace classad {

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
typedef classad_map<std::string,View*> PartitionedViews;
typedef classad_map<std::string,ViewMembers::iterator> MemberIndex;


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
	inline int	Size( ) const { return( (int)viewMembers.size( ) ); }
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

	/** A constant iterator we can use on a View */
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

}

#endif // __CLASSAD_VIEW_H__
