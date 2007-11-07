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

//******************************************************************************
// classifiedjobs.h
//
// Cai, Weiru
//
// class Class contains information about how to classify a job base on its
// requirements.
//
//******************************************************************************

#ifndef _CLASSIFIEDJOBS_H
#define _CLASSIFIEDJOBS_H

#include "condor_classad.h"

class Class
{
    public:

		Class(class ExprTree*);
		~Class();

		const ExprTree*	Definition();
		int				ClassRequired(ExprTree*);

		friend			class ClassList;

    private:

		ExprTree*		definition;	// definition of this class
		ClassAdList*	list;	// classads belong to this class
		Class*			next;
};

class ClassList
{
    public:

		ClassList() { classes = NULL; numOfClasses = 0; tail = NULL; }
		~ClassList();

		//int		BuildClassList(char*, char*);
		int		NumOfClasses() { return numOfClasses; }
		int		ClassRequired(int, ExprTree*);
		int		ClassIndex(ExprTree*);	// index of the class defined
		Class*	IndexClass(int);		// the class indexed

    private:

		Class*	classes;
		Class*	tail;
		int		numOfClasses;			// number of classes
};

#endif
