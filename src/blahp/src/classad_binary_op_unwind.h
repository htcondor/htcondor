/*
#
#  File :     classad_binary_op_unwind.h
#
#
#  Author :   Francesco Prelz ($Author: mezzadri $)
#  e-mail :   "francesco.prelz@mi.infn.it"
#
#  Revision history :
#   2-Mar-2006 Original release
#
#  Description:
#   Classad Unparser spaecialisation to extract the arguments
#   of binary operations of a given ExprTree.
#
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#
#
*/

#include "classad/classad_distribution.h"

#ifndef __CLASSAD_BINARY_OP_UNWIND_H__
#define __CLASSAD_BINARY_OP_UNWIND_H__

using namespace classad;

namespace classad {

class BinaryOpUnwind : public ClassAdUnParser
{
    private:
        // Counter for values appearing in each attribute inspected by 'member' function
        std::map<std::string, std::size_t> m_member_list_counter_;
	void collapse_quotes(std::string &arg);
    public:
          /// Constructor
        BinaryOpUnwind( );
          /// Destructor
        virtual ~BinaryOpUnwind( );
          /// Operator unparser
        virtual void UnparseAux( std::string &buffer, Operation::OpKind op, 
                                        ExprTree *op1, ExprTree *op2, ExprTree *op3 );
        virtual void UnparseAux( std::string &buffer, std::string &fnName, std::vector<ExprTree*>& args );


        std::vector<std::string> m_unwind_output;
};

} // end of classad namespace

#endif // defined __CLASSAD_BINARY_OP_UNWIND_H__
