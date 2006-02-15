/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __GMR_H__
#define __GMR_H__

#define WANT_NAMESPACES
#include "classad_distribution.h"

class GMR {
public:
    GMR( );
    ~GMR( );

    typedef std::set<std::string,classad::CaseIgnLTStr> Dependencies;

    struct Port {
        std::string          label;
        classad::ClassAd         *portAd;
        Dependencies    dependencies;
    };
    typedef vector<Port> Ports;

    classad::ClassAd             *parentAd;
    int                 key;
    Ports               ports;

    static GMR *MakeGMR( int, classad::ClassAd * );
    void Display( FILE * );
    bool GetExternalReferences( classad::References &refs );
	/*
    bool AddRectangles( Rectangles&, const References &imprefs, 
				const References &exprefs );
    bool AddRectangles( int port, Rectangles&, const References &imprefs, 
				const References &exprefs );
	*/
};

typedef std::multimap<int,classad::ClassAd*> ClassAdMap;
typedef std::map<int,GMR*> GMRMap;

#endif
