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

		int		BuildClassList(char*, char*);
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
