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


class ClassAd : public ExprTree
{
  public:
	// Ctors/dtor
	ClassAd ();
	ClassAd (char *);			// instantiate in domain
	ClassAd (const ClassAd &);
	~ClassAd ();

	// override methods
	virtual ExprTree *copy(CopyMode);
	virtual bool toSink (Sink &);	
	
	// Accessors/modifiers
	void		clear( );
	bool 		insert( char *, ExprTree * );
	ExprTree 	*lookup( char * );
	bool 		remove( char * );
	
	// evaluation methods
	void		evaluateAttr( char* , Value & );	// lookup+eval'n
	bool		evaluate( const char*, Value&, int=-1);// parse+eval (strings)
	bool		evaluate( char* , Value & , int=-1);// parse+eval'n (strBuffers)
	void		evaluate( ExprTree*, Value & );		// eval'n

	// flattening method
	bool		flatten( ExprTree*, Value&, ExprTree *& );
	
	// factory methods to procure classads
	static ClassAd *augmentFromSource 	(Source &, ClassAd &);
	static ClassAd *fromSource 			(Source &);

  protected:
	virtual void setParentScope( ClassAd* ad );

  private:
	friend	class AttributeReference;
	friend class ExprTree;
	friend class ClassAdIterator;

	virtual void _evaluate( EvalState& , EvalValue& );
	virtual bool _flatten( EvalState&, EvalValue&, ExprTree*&, OpKind* );

	static	ClassAdDomainManager 	domMan;
	ExtArray<Attribute> 			attrList;

	StringSpace		*schema;
	ClassAd			*parentScope;
	int				last;
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
