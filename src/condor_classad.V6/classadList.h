#ifndef __CLASSAD_LIST_H__
#define __CLASSAD_LIST_H__

#include "classad.h"

enum {CLASSAD_DUP, CLASSAD_ADOPT, CLASSAD_DISPOSE, CLASSAD_HANDOVER};

struct AdListNode
{
	AdListNode *prev, *next;
	ClassAd		*item;
};

	
class ClassAdList
{
	public:
		// ctor/dtor
		ClassAdList ();
		~ClassAdList ();

		// accessors/modifiers
		bool append (ClassAd *, int=CLASSAD_ADOPT);
		bool remove (ClassAd *, int=CLASSAD_DISPOSE);
		bool isMember(ClassAd *);
		void clear  (void);
		int	 length() { return len; }

		// dump function
		bool toSink(Sink &);

		// factory methods
		static ClassAdList *augmentFromSource (Source &, ClassAdList &);
		static ClassAdList *fromSource (Source &);
	
	private:
		friend class ClassAdListIterator;

		AdListNode	*head, *tail;
		int 		len;
};


class ClassAdListIterator
{
	public:
		// ctors/dtor
		ClassAdListIterator ();
		ClassAdListIterator (const ClassAdList &al);
		~ClassAdListIterator();

		// iteration functions
		void initialize (const ClassAdList &al);
		void toBeforeFirst ();
		void toAfterLast   ();
		bool nextClassAd   (ClassAd *&);
		ClassAd *nextClassAd();
		bool previousClassAd (ClassAd *&);
		ClassAd *previousClassAd();

		// predicates to check position
		inline bool isBeforeFirst () { return beforeFirst; }
		inline bool isAfterLast   () { return afterLast;   }

	private:
		const ClassAdList 	*list;
		const AdListNode	*node;
		bool				beforeFirst;
		bool				afterLast;
};

	
#endif//__CLASSAD_LIST_H__
