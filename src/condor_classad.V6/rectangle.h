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
#include "extArray.h"

typedef ExtArray<unsigned int> KeySet;
const int SUINT = (sizeof(unsigned int)*8);	// in bits

struct Interval {
	Interval() { key = -1; openLower = openUpper = false; }
	int		key;
	Value 	lower, upper;
	bool	openLower, openUpper;
};


typedef map<int, int> KeyMap;
typedef map<string, set<int>, CaseIgnLTStr > DimRectangleMap;
typedef map<int, Interval> OneDimension;
typedef map<string, OneDimension, CaseIgnLTStr> AllDimensions;


class Rectangles {
public:
	Rectangles( );
	~Rectangles( );

	void Clear( );

	int NewClassAd( int id=-1 );
	int NewPort( int id=-1 );
	int NewRectangle( int id=-1 );

	int AddUpperBound(const string&attr,Value& val,bool open, bool constraint,
				int rkey=-1);
	int AddLowerBound(const string&attr,Value& val,bool open, bool constraint,
				int rkey=-1);
	void AddExportedVerificationRequest( int rkey=-1 );
	void AddImportedVerificationRequest( const string&, int rkey=-1 );
	void AddUnexportedDimension( const string&, int rkey=-1 );

	void Typify( const References &exported, int srec, int erec );
	void Display( FILE * );

	enum { NO_ERROR, INCONSISTENT_TYPE, INCONSISTENT_VALUE };

	KeyMap			rpMap, pcMap, crMap;
	set<int>		verifyExported;
	DimRectangleMap	verifyImported;
	DimRectangleMap unexported, unimported;
	AllDimensions 	tmpExportedBoxes, tmpImportedBoxes;
	AllDimensions 	exportedBoxes, importedBoxes;
	int				cId, pId, rId;

private:
	static bool NormalizeInterval( Interval&, char );
	static char GetIndexPrefix( const Value::ValueType );
};

#endif // RECTANGLE_H
