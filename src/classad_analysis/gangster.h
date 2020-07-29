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


#ifndef __GANGSTER_H__
#define __GANGSTER_H__

#include "classad/classad_distribution.h"
#include "gmr.h"

class Gangster {
public:
	Gangster( Gangster*, int );
	virtual ~Gangster( );

	void	assign( GMR* );
	bool	setupMatchBindings( int );
	void	teardownMatchBindings( int );
	bool	match( bool );
	void	retire( bool=false );
	void	reset( );
	void	printKeys( );

	void	testOneWayMatch( int, classad::Value& );

protected:
	void	testMatch( int, classad::Value& );
	bool 	fillDock( int, bool );
	bool	fillDockIndexed( int, bool );
	static  bool indexed;

	struct Dock {
		GMR::Port		*port;
		int				lastKey;
		Gangster		*boundGangster;
		int				boundDock;
		bool			satisfied;
	};
	typedef std::vector<Dock> Docks;

	static int			nextContextNum;

	GMR			*gmr;
	Gangster 	*parentGangster;
	int			parentDockNum;
	classad::ClassAd		*context;
	int			contextNum;
	int			parentLinked;
	Docks		docks;
};


#endif
