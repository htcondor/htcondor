#ifndef __CLASSAD_H__
#define __CLASSAD_H__

#include "exprTree.h"
#include "extArray.h"
#include "stringSpace.h"
#include "sink.h"
#include "source.h"


// each attribute in the classad
class Attribute
{
	public:
		Attribute ();

		bool		valid;
		char 		*attrName;
		ExprTree	*expression;
		SSString	canonicalAttrName;
};


class ClassAd
{
  public:
	// Ctors/dtor
	ClassAd ();
	ClassAd (char *);			// instantiate in domain
	ClassAd (const ClassAd &);
	~ClassAd ();

	// Accessors/modifiers
	bool 		insert (char *, ExprTree *);
	ExprTree 	*lookup (char *);
	bool 		remove (char *);
	
	// dump function
	bool	toSink (Sink &);	
	
	// factory methods to procure classads
	static ClassAd *augmentFromSource 	(Source &, ClassAd &);
	static ClassAd *fromSource 			(Source &);

  private:
	friend class ExprTree;
	friend class ClassAdIterator;

	ExtArray<Attribute> attrList;
	StringSpace	*schema;
	int			last;
};


// Iterator
class ClassAdIterator
{
	public:
		// ctors/dtor
		ClassAdIterator () { ad = NULL ; index = -1; }
		ClassAdIterator (const ClassAd &ca) { ad = &ca; index = -1; }	
		~ClassAdIterator();

		// iteration functions
		inline void initialize (const ClassAd &ca) { ad = &ca ; index = -1; }
		inline void toBeforeFirst () { index = -1; }
		inline void toAfterLast ()	{ index = ad->last; }
		bool nextAttribute (char *&, ExprTree *&);
		bool previousAttribute (char *&, ExprTree *&);

		// predicates to check position
		inline bool isBeforeFirst () { return (index == -1); }
		inline bool isAfterLast () { return (ad ? (index==ad->last) : false); }

	private:
		int		index;
		const ClassAd	*ad;
};


#endif//__CLASSAD_H__
