/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __ATTRREFS_H__
#define __ATTRREFS_H__

#include "stringSpace.h"

BEGIN_NAMESPACE( classad )

/** Represents a attribute reference node in the expression tree
*/
class AttributeReference : public ExprTree 
{
  	public:
		/** Constructor
		*/
    	AttributeReference ();

		/**  Destructor
		*/
    	~AttributeReference ();

        /** Sends the attribute reference object to a Sink.
            @param s The Sink object.
            @return false if the expression could not be successfully
                unparsed into the sink.
            @see Sink
        */
		virtual bool ToSink (Sink &s );

		/** Set the attribute reference.
			@param expr The expression part of the reference (i.e., in
				case of expr.attr).  This parameter is NULL if the reference
				is absolute (i.e., .attr) or simple (i.e., attr).
			@param attrName The name of the reference.  This string is
				duplicated internally.
			@param absolute True if the reference is an absolute reference
				(i.e., in case of .attr).  This parameter cannot be true if
				expr is not NULL
		*/
    	void SetReference (ExprTree *expr, char *attrName, bool absolute=false);

		virtual AttributeReference* Copy( );

  	private:
		// private ctor for internal use
		AttributeReference( ExprTree*, char*, bool );

		virtual void _SetParentScope( ClassAd* p );
    	virtual bool _Evaluate( EvalState & , Value & );
    	virtual bool _Evaluate( EvalState & , Value &, ExprTree*& );
    	virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* );

		int	FindExpr( EvalState&, ExprTree*&, ExprTree*&, bool );

		ExprTree	*expr;
		bool		absolute;
    	char        *attributeStr;
    	SSString    attributeName;
};

END_NAMESPACE // classad

#endif//__ATTRREFS_H__
