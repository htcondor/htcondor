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
