#include <std.h>
#include <string.h>
#include <assert.h>
#include <iostream.h>
#include "ast.h"
#include "parser.h"
#include "classad_util.h" 

//
// Given a ClassAd and a ClassAdList, a pointer to the first match in the 
// list is returned. 
// If match found, 1 returned. Otherwise, 0 returned.
//
int FirstMatch(ClassAd *ad, ClassAdList *adList, ClassAd * &returnAd)
{
    ClassAd *temp;
    if(!ad || !adList)
    {
        returnAd = NULL;                     // no match.
        return 0;
    }
    adList->OpenList();
    for(temp = adList->NextClassAd(); temp; temp = adList->NextClassAd())
    {
        if(ad->IsAMatch(temp))
	{                                    // match found.
	    returnAd = temp;
	    return 1;
	}
    }
    adList->CloseList();
    
    returnAd = NULL;                         // match not found.
    return 0;
}

//
// Given a ClassAd and a ClassAdList, a list of pointers to the matches in the
// list is returned. 
// If match(es) found, 1 returned. Otherwise, 0 returned.
//
int AllMatch(ClassAd *ad, ClassAdList *adList, ClassAdList *&returnList)
{
    ClassAd *temp;
    returnList = NULL;
    if(!ad || !adList)
    {
        returnList = NULL;
        return 0;                            // no match.
    }
    
    adList->OpenList();
    for(temp = adList->NextClassAd(); temp; temp = adList->NextClassAd())
    {
        if(ad->IsAMatch(temp))
	{                                    // match found.
	    if(!returnList)
	    {
	        returnList = new ClassAdList;
		if(!returnList)
		{
		    cerr << "Warning : you ran out of space -- quitting !" << endl;
		    exit(1);
		}
	    }
	    returnList->Insert(temp);        // make up the returned list.
	}
    }
    adList->CloseList();
    
    if(returnList)
    {
        return 1;                            // match(es) found.
    }
    else
    {
        return 0;                            // match not found. 
    }
}

//
// Given a constraint, determine whether the first ClassAd is > the second 
// one, in the sense whether the constraint is evaluated to be TRUE. 
// The constraint should look just like a "Requirement". Every variable has 
// a prefix "MY." or "TARGET.". A variable with prefix "MY." is should be 
// evaluated against the first ClassAd and a variable with prefix "TARGET."
// should be evaluated against the second ClassAd.
// If >, return 1;
// else, return 0. (Note it can be undefined.)  
//
int IsGT(ClassAd *ad1, ClassAd *ad2, char *constraint)
{
    ExprTree *tree;
    EvalResult *val;

    char *buffer;

    if(!ad1 || !ad2 || !constraint)
    {
        return 0;
    }

    buffer = new char[strlen(constraint)+1];
    if(!buffer)
    {
        cerr << "Warning : you ran out of memory -- quitting !" << endl;
	exit(1);
    }
    strcpy(buffer, constraint);

    val = new EvalResult;
    if(val == NULL)
    {
        cerr << "Warning : you ran out of memory -- quitting !" << endl;
        exit(1);
    }

    if(!Parse(buffer, tree))
    {                                        // parse the constraint.
        if(tree->MyType() == LX_ERROR)
	{
	    cerr << "Parse error in the input string -- quitting !" << endl;
	    exit(1);
	}
    }
    else
    {
        cerr << "Parse error in the input string -- quitting !" << endl;
	exit(1);
    }
    tree->EvalTree(ad1, ad2, val);           // evaluate the constraint.
    if(!val || val->type != LX_BOOL)
    {
        delete []buffer;
        delete tree;
        delete val;
        return 0;
    }
    else
    {
        if(!val->b)
        {
	    delete []buffer;
            delete tree;
            delete val;
            return 0; 
        }
    }  

    delete []buffer;
    delete tree;
    delete val;
    return 1;   
}

//
// Sort a list according to a given constraint in ascending order.
//
void SortClassAdList(ClassAdList *adList1, ClassAdList *&adList2, char *constraint)
{
    int *mark;                               // mark each entity in the list
                                             // already selected.
    ClassAd *temp, *min;
    if(!adList1 || !adList1->MyLength())
    {
        cerr << "Warning : empty list not sorted !" << endl;
	adList2 = NULL;
	return;
    }
    
    mark = new int[adList1->MyLength()];
    if(!mark)
    {
        cerr << "Warning : you ran out of space -- quitting !" << endl;
	exit(1);
    }
    
    adList2 = new ClassAdList;
    if(!adList2)
    {
        cerr << "Warning : you ran out of space -- quitting !" << endl;
	exit(1);
    }

    for(int i = 0; i < adList1->MyLength(); i++)
    {
        mark[i] = 0;                         // initialize as unselected.
    }

    int j;
    int index;
    for(i = 0; i < adList1->MyLength(); i++)
    {
        adList1->OpenList();
	min = NULL;
	for(temp = adList1->NextClassAd(), j = 0; temp;
	    temp = adList1->NextClassAd(), j++)
	{
	    if(mark[j])
	    {                                // skip if already selected.
	        continue;
	    }
	    else
	    {
	        if(!min)
		{
		    min = temp; 
		    index = j;
		    continue;
		}
		else
		{
		    if(IsGT(temp, min, constraint))
		    {
		        ;
		    }
		    else
		    {
		        min = temp;          // keep the smaller.
			index = j;
		    }
		}
	    }
	}
	mark[index] = 1;                     // mark the selected one.
	adList2->Insert(min);                // make up the sorted list.
    }
    delete []mark;
}










