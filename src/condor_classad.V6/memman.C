#include "condor_common.h"
#include "exprTree.h"
#include "memman.h"

ParserMemoryManager::
ParserMemoryManager ()
{
	lastSlot = 0;
	exprList.fill (NULL);
}


ParserMemoryManager::
~ParserMemoryManager ()
{
	finishedParse ();
}


int ParserMemoryManager::
doregister (ExprTree *tree)
{
	int i = 0;

	// Search for holes in the list
	while (i < lastSlot && exprList[i] != NULL)
	{
		i++;
	}
	
	// No holes; grow list (note that 'i' points to the new slot)
	if (i == lastSlot)
		lastSlot++;

	exprList[i] = tree;
	
	return i;
}


void ParserMemoryManager::
unregister (int slot)
{
	exprList[slot] = NULL;
}


void ParserMemoryManager::
finishedParse ()
{
	// Go through exprList and delete any entries that are not
	// NULL.  Also, reinitialize the list
	for (int i = 0; i < lastSlot; i++)
	{
		if (exprList[i] != NULL)
		{
			delete exprList[i];
		}

		exprList[i] = NULL;
	}

	lastSlot = 0;
}


ExprTree *ParserMemoryManager::
getExpr (int id)
{
	return exprList[id];
}
