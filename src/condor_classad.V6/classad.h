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
		virtual bool toSink (Sink &);	
		virtual void setParentScope( ClassAd* );
	
		// Accessors/modifiers
		void		clear( );
		bool 		insert( char *, ExprTree * );
		bool		insert( char *, char *, int=-1 );
		ExprTree 	*lookup( char * );
		bool 		remove( char * );
	
		// evaluation methods
		void		evaluateAttr( char* , Value & );			// lookup+eval'n
		bool		evaluateExpr( char* , Value & , int=-1);	// parse+eval'n 
		void		evaluateExpr( ExprTree*, Value & );			// eval'n
		void		evaluateExpr( ExprTree*, Value &, ExprTree *& );

		bool		evaluateAttrInt( char*, int& );
		bool		evaluateAttrReal( char*, double& );
		bool		evaluateAttrNumber( char*, int& );
		bool		evaluateAttrNumber( char*, double& );
		bool		evaluateAttrString( char*, char*, int );
		bool		evaluateAttrString( char*, char*& );

		// flattening method
		bool		flatten( ExprTree*, Value&, ExprTree *& );
		
		// factory methods to procure classads
		static ClassAd *augmentFromSource 	(Source &, ClassAd &);
		static ClassAd *fromSource 			(Source &);

  	private:
		friend 	class AttributeReference;
		friend 	class ExprTree;
		friend 	class ClassAdIterator;

		virtual ExprTree* _copy( CopyMode );
		virtual void _evaluate( EvalState& , Value& );
		virtual void _evaluate( EvalState&, Value&, ExprTree*& );
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );
	
		static	ClassAdDomainManager 	domMan;
		ExtArray<Attribute> 			attrList;
		ClassAd							*parentScope;
	
		StringSpace		*schema;
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
