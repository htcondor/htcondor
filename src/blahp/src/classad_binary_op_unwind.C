/*
#
#  File :     classad_binary_op_unwind.C
#
#
#  Author :   Francesco Prelz ($Author: mezzadri $)
#  e-mail :   "francesco.prelz@mi.infn.it"
#
#  Revision history :
#   3-Mar-2006 Original release
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

#include <vector>
#include <string>
#include <sstream>

#include <strings.h> // strcasecmp

#include "classad/classad_distribution.h"
#include "classad_binary_op_unwind.h"

#if 1
namespace classad {
#endif

// BinaryOpUnwind object implementation
BinaryOpUnwind::
BinaryOpUnwind( )
{
}


BinaryOpUnwind::
~BinaryOpUnwind( )
{
}

void BinaryOpUnwind::collapse_quotes(std::string &arg)
{
  size_t qpos;

  while ((qpos = arg.find_first_of('\'')) != std::string::npos)
   {
    arg.erase(qpos,1);
   }
}

void BinaryOpUnwind::
UnparseAux(std::string &buffer,Operation::OpKind op, ExprTree *t1, ExprTree *t2,
	   ExprTree *t3)
{
  // We are interested just in a subset of binary operations

  if ( op == Operation::LESS_THAN_OP ||
       op == Operation::GREATER_THAN_OP ||
       op == Operation::LESS_OR_EQUAL_OP ||
       op == Operation::GREATER_OR_EQUAL_OP ||
       op == Operation::EQUAL_OP ||
       op == Operation::IS_OP )
   {
    // Check that we have one attribute reference and one literal.
    ExprTree *attr = NULL, *value = NULL;
    bool args_ok = false;
    bool value_at_right;
    bool numeric_value;
    std::string attribute_name;
    std::string attribute_value; 

    if ((dynamic_cast<Literal *>(t1) != nullptr) &&
        t2->GetKind() == ExprTree::ATTRREF_NODE)
     {
      args_ok = true;
      attr = t2;
      value = t1;
      value_at_right = false;
     }
    if ((dynamic_cast<Literal *>(t2) != nullptr) &&
        t1->GetKind() == ExprTree::ATTRREF_NODE)
     {
      args_ok = true;
      attr = t1;
      value = t2;
      value_at_right = true;
     }

    numeric_value = false;

    if (args_ok)
     {
      ExprTree *expr;
      bool    absolute;
      // We just care about the attribute name, not about its base ad.
      ((AttributeReference*)attr)->GetComponents(expr, attribute_name, absolute);

      Value v;
      EvalState state;
      //state.SetScopes( ad );

      value->Evaluate(state,v);
      int intres;
      std::string strres;

      if (v.IsIntegerValue( intres ))
       {
        Unparse( attribute_value, v );
        numeric_value = true;
       }
      else if (v.IsStringValue( strres ))
       {
        // There's extra quoting rules in the string Unparse function.
        Unparse( attribute_value, v );
       }
      else
       {
        args_ok = false;
       }
     }

    collapse_quotes(attribute_value);

    /* Comparison on numeric value ?*/
    if ((!numeric_value) &&
         op != Operation::EQUAL_OP && op != Operation::IS_OP)
      args_ok = false;

    if (args_ok)
     {
      if ( ( value_at_right  && ( op == Operation::LESS_THAN_OP || op == Operation::LESS_OR_EQUAL_OP ) ) ||
           ( !value_at_right && ( op == Operation::GREATER_THAN_OP || op == Operation::GREATER_OR_EQUAL_OP ) ) )
       {
        std::string result_line = attribute_name;
        result_line.append("_Max='");
        result_line.append(attribute_value);
        result_line.append("'");
        m_unwind_output.push_back(result_line);
       }

      if ( ( !value_at_right  && ( op == Operation::LESS_THAN_OP || op == Operation::LESS_OR_EQUAL_OP ) ) ||
           ( value_at_right && ( op == Operation::GREATER_THAN_OP || op == Operation::GREATER_OR_EQUAL_OP ) ) )
       {
        std::string result_line = attribute_name;
        result_line.append("_Min='");
        result_line.append(attribute_value);
        result_line.append("'");
        m_unwind_output.push_back(result_line);
       }

      if ( op == Operation::EQUAL_OP ||
           op == Operation::IS_OP )
       {
        std::string result_line = attribute_name;
        result_line.append("='");
        result_line.append(attribute_value);
        result_line.append("'");
        m_unwind_output.push_back(result_line);
       }

     }
    
   }

  // Fall back to base class to complete unparsing.
  ClassAdUnParser::UnparseAux( buffer, op, t1, t2, t3 );
  return;
}

void BinaryOpUnwind::
UnparseAux( std::string &buffer, std::string &fnName, std::vector<ExprTree*>& args )
{
  if (strcasecmp(fnName.c_str(),"member") == 0)
   {
    if ((dynamic_cast<Literal *>(args[0]) != nullptr) &&
        (args[1])->GetKind() == ExprTree::ATTRREF_NODE)
     {
      std::string attribute_name;
      std::string attribute_value;

      ExprTree *expr;
      bool    absolute;
      // We just care about the attribute name, not about its base ad.
      ((AttributeReference*)args[1])->GetComponents(expr, attribute_name, absolute);

      Value v;
      EvalState state;

      args[0]->Evaluate(state,v);
      Unparse( attribute_value, v );
      collapse_quotes(attribute_value);

      std::stringstream result_line;
      result_line << attribute_name << "[" << m_member_list_counter_[attribute_name]++ << "]='" << attribute_value << "'";
      m_unwind_output.push_back(result_line.str());
     }
   }

  // Fall back to base class to complete unparsing.
  ClassAdUnParser::UnparseAux( buffer, fnName, args );
  return;
}

#if 1
} // end of classad namespace
#endif
