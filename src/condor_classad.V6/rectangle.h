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

#if !defined RECTANGLE_H
#define RECTANGLE_H

#include <map>
#include <set>
#include "value.h"

class KeySet {
public:
	KeySet( int size=32 );
	~KeySet( );

	void Display( );
	void Clear( );
	void MakeUniversal( );
	void Insert( const int );
	bool Contains( const int );
	void Remove( const int );
	int  Cardinality( );
	void Intersect( KeySet& );
	void Unify( KeySet& );
	void Subtract( KeySet& );
	void IntersectWithUnionOf( KeySet&, KeySet& );
	void IntersectWithUnionOf( KeySet&, KeySet&, KeySet& );

	class iterator {
		public:
			iterator( );
			~iterator( );
			void Initialize( KeySet& );
			bool Next( int& );
		private:
			KeySet 	*ks;
			int		index, offset;
			int		last;
	};
private:

	friend class iterator;
	static const char   numOnBits[256];
	static const int 	SUINT;
	bool				universal;
	vector<unsigned>	kset;
};


struct Interval {
	Interval() { key = -1; openLower = openUpper = false; }
	int		key;
	Value 	lower, upper;
	bool	openLower, openUpper;
};


typedef std::map<int, int> KeyMap;
typedef std::map<std::string, KeySet, CaseIgnLTStr > DimRectangleMap;
typedef std::map<int, Interval> OneDimension;
typedef std::map<std::string, OneDimension, CaseIgnLTStr> AllDimensions;
typedef std::map<std::string, int, CaseIgnLTStr> Representatives;
typedef std::map<int, std::set<int> > Constituents;
typedef std::map<int, std::string> ImportedSignatures;
typedef std::map<std::string, ImportedSignatures, CaseIgnLTStr> DeviantImportedSignatures;
class  QueryProcessor;


class Rectangles {
public:
	Rectangles( );
	virtual ~Rectangles( );

	virtual void Clear( );
	virtual bool Empty( ) { return empty; }

	int NewClassAd( int id=-1 );
	int NewPort( int id=-1 );
	int NewRectangle( int id=-1 );
	int AddUpperBound(const std::string&dim,Value& v,bool open,bool exp,int rkey=-1);
	int AddLowerBound(const std::string&dim,Value& v,bool open,bool exp,int rkey=-1);
	void AddDeviantExported( int rkey=-1 );
	void AddDeviantImported( const std::string&, int rkey=-1 );
	void AddUnexportedDimension( const std::string&, int rkey=-1 );
	bool AddRectangles( ClassAd*, const std::string&, bool );
	void Complete( bool );

	bool Summarize( Rectangles&, Representatives&, Constituents&, KeyMap& );
	bool Augment( Rectangles &rec1, Rectangles &rec2, const ClassAd *ad,
				    const std::string &label );

	void Display( FILE * );
	
	enum { NO_ERROR, INCONSISTENT_TYPE, INCONSISTENT_VALUE };

	bool MapRectangleID( int rId, int &portNum, int &pId, int &cId );
	bool MapPortID( int pId, int &portNum, int &cId );
	bool UnMapClassAdID( int cId, int &bdId, int &erId );

	void PurgeRectangle( int );

	KeyMap			rpMap, pcMap, crMap;
	int				cId, pId, rId;
	AllDimensions 	tmpExportedBoxes, tmpImportedBoxes;

	AllDimensions 	exportedBoxes;
	DimRectangleMap unexported; 
	KeySet			deviantExported;
	DimRectangleMap allExported;

	AllDimensions	importedBoxes;
	DimRectangleMap	unimported;
	DimRectangleMap	deviantImported;

	DeviantImportedSignatures	devImpSigs;

	static References exportedReferences;
	static References importedReferences;

private:
	bool		empty;
	static bool Rename(const ClassAd *port,const std::string &attr, 
					const std::string &label, std::string &renamed );
	static bool NormalizeInterval( Interval&, char );
	static char GetIndexPrefix( const Value::ValueType );
};

#endif // RECTANGLE_H
