#include "classadList.h"

ClassAdList::
ClassAdList()
{
	len  = 0;
	head = NULL;
	tail = NULL;
}


ClassAdList::
~ClassAdList()
{
	clear ();
}

void ClassAdList::
clear (void)
{
	AdListNode *prev = head, *curr = head;

	if (head)
	{
		while (curr)
		{
			prev = curr;
			curr = curr->next;
			delete prev->item;
			delete prev;
		}
	}	
	
	head = NULL;
	tail = NULL;
}


bool ClassAdList::
append (ClassAd *ad, int mode)
{
	AdListNode 	*node = new AdListNode;
	ClassAd		*newAd;

	if (!node) return false;

	newAd = (mode == CLASSAD_DUP) ? newAd = new ClassAd (*ad) : ad;
	if (!newAd)
	{
		delete node;
		return false;
	}

	len ++;	
	node->next = NULL;
	node->prev = tail;
	node->item = ad;

	if (tail)
	{
		tail->next = node;
		tail = node;
	}
	else
	{
		head = node;
		tail = node;
	}

	return true;
}


bool ClassAdList::
remove (ClassAd *ad, int mode)
{
	AdListNode 	*node = head;

	while (node)
	{
		if (node->item == ad)
			break;
		node = node->next;
	}
	if (!node) return false;


	if (node == head && node == tail)
	{
		head = NULL;
		tail = NULL;
	}
	else
	if (node == head)
	{
		head = head->next;
		head->prev = NULL;
	}	
	else
	if (node == tail)
	{
		tail = tail->prev;
		tail->next = NULL;
	}
	else
	{
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	delete node;
	if (mode == CLASSAD_DISPOSE) delete ad;

	len --;

	return true;
}


bool ClassAdList::
isMember (ClassAd *ad)
{
	AdListNode 	*node = head;

	while (node)
	{
		if (node->item == ad) return true;
		node = node->next;
	}

	return false;
}

bool ClassAdList::
toSink (Sink &s)
{
    AdListNode  *node = head;

	if (!s.sendToSink ((void*)"{\n", 2)) return false;

    while (node)
    {
		if (!node->item->toSink(s)) return false;
        node = node->next;
    }

	return (s.sendToSink ((void*)"}\n", 2));
}

ClassAdList *ClassAdList::
fromSource (Source &s)
{
    ClassAdList *adList = new ClassAdList;

    if (!adList) return NULL;
    if (!s.parseClassAdList (*adList)) 
    {
        delete adList;
        return (ClassAdList*)NULL;
    }

    // ... else 
    return adList;
}

ClassAdList *ClassAdList::
augmentFromSource (Source &s, ClassAdList &adList)
{
    return (s.parseClassAdList(adList) ? &adList : NULL);
}




ClassAdListIterator::
ClassAdListIterator ()
{
	list = NULL;
	node = NULL;
	beforeFirst = true;
	afterLast = true;
}


ClassAdListIterator::
ClassAdListIterator (const ClassAdList &al)
{
	list = &al;
	node = NULL;
	beforeFirst = true;
	afterLast = false;
}


ClassAdListIterator::
~ClassAdListIterator()
{
}



void ClassAdListIterator::
initialize (const ClassAdList &al)
{
	list = &al;
	node = NULL;
	beforeFirst = true;
	afterLast = false;
}



void ClassAdListIterator::
toBeforeFirst ()
{
	node = NULL;
	beforeFirst = true;
	afterLast = false;
}



void ClassAdListIterator::
toAfterLast ()
{
	node = NULL;
	beforeFirst = false;
	afterLast = true;
}



bool ClassAdListIterator::
nextClassAd (ClassAd *&ad)
{
	if (!list) return false;

	if (beforeFirst)
	{
		beforeFirst = false;
		node = list->head;
	}
	else
	if (node == list->tail)
	{
		afterLast = true;
		node = NULL;
		return false;
	}
	else
	if (afterLast)
		return false;
	else
		node = node->next;

	ad = node->item;
	return true;
}



ClassAd *ClassAdListIterator::
nextClassAd ()
{
	ClassAd *ad;
	return (nextClassAd (ad) ? ad : NULL);
}



bool ClassAdListIterator::
previousClassAd (ClassAd *&ad)
{
	if (!list) return false;

	if (beforeFirst)
		return false;
	else
	if (afterLast)
	{
		afterLast = false;
		node = list->tail;
	}
	else
	if (node == list->head)
	{
		beforeFirst = true;
		node = NULL;
		return false;
	}
	else
		node = node->prev;

	ad = node->item;
	return true;
}



ClassAd *ClassAdListIterator::
previousClassAd ()
{
	ClassAd *ad;
	return (previousClassAd(ad) ? ad : NULL);
}

