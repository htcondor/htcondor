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


#ifndef __CLASSAD_EXPR_LIST_H__
#define __CLASSAD_EXPR_LIST_H__

#include <vector>

namespace classad {

/// Represents a list of expressions, like {1, 2, 3}
class ExprList : public ExprTree
{
	public:
		ExprList();
		ExprList(const std::vector<ExprTree*>& exprs);

        /// Copy Constructor
        ExprList(const ExprList &other_list);

		/// Destructor
		virtual ~ExprList();

        ExprList &operator=(const ExprList &other_list);

		/// node type
		virtual NodeKind GetKind (void) const { return EXPR_LIST_NODE; }

		/** Factory to make an expression list expression
		 * 	@param	list A vector of the expressions to be contained in the
		 * 		list
		 * 	@return The constructed expression list
		 */
		static ExprList *MakeExprList( const std::vector< ExprTree* > &list );

		/** Deconstructor to obtain the components of an expression list
		 * 	@param list The list of expressions
		 */
		void GetComponents( std::vector<ExprTree*>& list ) const;

		// Like Clear(), but transfers ownership to `list`.
		void removeAll( std::vector<ExprTree*>& list );

		virtual ExprTree* Copy( ) const;

        bool CopyFrom(const ExprList &other_list);

        virtual bool SameAs(const ExprTree *tree) const;

        friend bool operator==(ExprList &list1, ExprList &list2);

		// STL-like iterators and functions
	    typedef std::vector<ExprTree*>::iterator       iterator;
    	typedef std::vector<ExprTree*>::const_iterator const_iterator;

        int size() const             { return (int)exprList.size();  }
        iterator begin()             { return exprList.begin(); }
		iterator end()               { return exprList.end();   }
		const_iterator begin() const { return exprList.begin(); }
		const_iterator end() const   { return exprList.end();   }

		void insert(iterator it, ExprTree* t);
		void push_back(ExprTree* t);
		void erase(iterator it);
		void erase(iterator f, iterator l);
	
		virtual const ClassAd *GetParentScope( ) const { return( parentScope ); }

    	bool GetValueAt(int location, Value& v, EvalState *es=nullptr) const;

	private:
		const ClassAd   *parentScope;

		std::vector<ExprTree*> exprList;

		void Clear (void);
		virtual void _SetParentScope( const ClassAd* p );
		virtual bool _Evaluate (EvalState &, Value &) const;
		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
        void CopyList(const std::vector<ExprTree*> &exprs);
};
} // classad

#endif//__CLASSAD_EXPR_LIST_H__
