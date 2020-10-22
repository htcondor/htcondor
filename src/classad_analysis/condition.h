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


#ifndef __CONDITION_H__
#define __CONDITION_H__

#include "boolExpr.h"

/** A BoolExpr in Conjunctive Form, which consistes of an attribute, a
	comparison operator and a literal value. Currently the only way to
	initialize a Conditions is with the ExprToMultiProfile method in the
	BoolExpr class.
	@see BoolExpr
*/
class Condition : public BoolExpr
{
	friend class BoolExpr;
 public:
		//
	enum AttrPos {
			/** indicatss attr op value form */ ATTR_POS_LEFT,
			/** indicates value op attr form */ ATTR_POS_RIGHT,
	};

		/** Default Constructor */
	Condition( );

		/** Destructor */
	~Condition( );

		/** Gets the attribute part of the Condition.
			@param result The attribute in string form.
			@return true on success, false on failure.
		*/
	bool GetAttr( std::string &result );

		/** Gets the operator part of the Condition. (or the operator part of
			the first simple condition in a complex single attribute
			Condition.)
			@param result The operator in the form of an Operation::Opkind
			@return true on success, false on failure.
		*/
	bool GetOp( classad::Operation::OpKind &result );

		/** Gets the operator part of the second simple condition in a complex
			single attribute Condition.
			@param result The operator in the form of an Operation::Opkind
			@return true on success, false on failure - or if Condition is
			not a complex single attribute Condition. 
		*/
	bool GetOp2( classad::Operation::OpKind &result );

		/** Gets the literal value part of the Condition. (or the operator part
			of the first simple condition in a complex single attribute
			Condition.)
			@param result The literal as a Value.
			@return true on success, false on failure.
		*/
	bool GetVal( classad::Value &result );

		/** Gets the literal value part of the second simple condition in a
			complex single attributeCondition.
			@param result The literal as a Value.
			@return true on success, false on failure.
		*/
	bool GetVal2( classad::Value &result );

		/** Gets the type of the literal value part of the Condition.  It is
			assumed that in a valid context the attribute will evaluate to a
			literal value of the same type.
			@param result The type as a Value::Value.
			@return true on success, false on failure.
		*/
	bool GetType( classad::Value::ValueType &result );

		/** Gets the position of the attribute (attr op value) or (value op 
			attr).
			@param result The attribute position as an AttrPos.
			@return true on success, false on failure.
		*/
	bool GetAttrPos( AttrPos &result );

		/** Determines whether the Condition is a complex Condition
			@return true if Condition is complex, false if it is normal
		*/
	bool IsComplex( ) const;

		/** Determines whether the Condition is a complex Condition with
			multiple attributes;
			@return true if Condition is complex and has multiple attributes,
			false if it is normal or has only one attribute.
		*/
	bool HasMultipleAttrs( ) const;

		/** A repository for annotations for the Profile.
			@see ConditionExplain
		*/
	ConditionExplain explain;
 private:
	std::string attr;
	classad::Operation::OpKind op;
	classad::Value val;
	classad::Operation::OpKind op2;
	classad::Value val2;
	AttrPos pos;
	bool isComplex;
	bool multiAttr;
	bool Init( const std::string &, classad::ExprTree *, bool );
	bool Init( const std::string &, classad::Operation::OpKind,
			   const classad::Value &,
			   classad::ExprTree *, AttrPos );
	bool InitComplex( classad::ExprTree * );
	bool InitComplex( const std::string &_attr,
					  classad::Operation::OpKind _op1,
					  const classad::Value &_val1,
					  classad::Operation::OpKind _op2,
					  const classad::Value &_val2,
					  classad::ExprTree * );
};

#endif //__CONDITION_H__
