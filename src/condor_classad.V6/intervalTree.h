/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

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
	int					rootT;

	static bool InsertInLeftSecondary(Secondary *&, const int &, double, bool);
	static bool InsertInRightSecondary(Secondary *&, const int &, double, bool);
	static bool DeleteFromSecondary(Secondary*&, const int &, double, bool );
	void VisitActive( int, KeySet& );
};

#endif
