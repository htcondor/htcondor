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

#if !defined(INTERVAL_TREE_H)
#define INTERVAL_TREE_H
#include <stdio.h>
#include <vector>
#include <list>
#include <set>
#include "rectangle.h"

class IntervalTree
{
public:
	IntervalTree( );
	~IntervalTree( );

	static IntervalTree *MakeIntervalTree( const OneDimension& );
	bool DeleteInterval( const int&, const Interval&  );
	bool WindowQuery( const Interval&, KeySet& keys );

	void Display( FILE *fp );

private:
	struct Secondary {
		double 		value;
		bool		open;
		int			key;
		Secondary	*next;
	};
	struct IntervalTreeNode {
		IntervalTreeNode(){ 
			nodeValue = max = min = 0; 
			LS = RS = NULL; 
			LT = RT = -1;
			active  = false;
			activeDesc = false;
			closestActive = -1;
		}
		double 		nodeValue, max, min;
		bool		active, activeDesc;
		int			closestActive, LT, RT;
		Secondary 	*LS, *RS;
	};

	IntervalTreeNode 	*nodes;
	int					size;

	static bool InsertInLeftSecondary(Secondary *&, const int &, double, bool);
	static bool InsertInRightSecondary(Secondary *&, const int &, double, bool);
	static bool DeleteFromSecondary(Secondary*&, const int &, double, bool );
	void VisitActive( int, KeySet& );
};

#endif
