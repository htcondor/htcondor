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


#ifndef __GMR_H__
#define __GMR_H__

#include "classad/classad_distribution.h"

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
    typedef std::vector<Port> Ports;

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
