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

struct NumericInterval {
	int		key;
	double 	lower, upper;
	bool	openLower, openUpper;
};

typedef map<int, NumericInterval> OneDimension;
typedef map<string, OneDimension, CaseIgnLTStr> AllDimensions;

class Rectangles {
	public:
		Rectangles( );
		~Rectangles( );

		bool AddUpperBound( int key, const string& attr, const double& val, 
					bool open );
		bool AddLowerBound( int key, const string& attr, const double& val,
					bool open );
		void Display( FILE * );

		AllDimensions boxes;
};

	// maps rectangle keys to classad keys
typedef map<int, int> KeyMap;

#endif // RECTANGLE_H
