/*
#  File :     classad_c_helper.C
#
#
#  Author :   Francesco Prelz ($Author: fprelz $)
#  e-mail :   "francesco.prelz@mi.infn.it"
#
#  Revision history :
#   5-Apr-2004 Original release
#  16-Apr-2004 Added string list parse and unparse.
#   7-May-2004 Added retrieval of string argument into dynamic string.
#  19-Aug-2004 Added boolean attribute.
#  30-Nov-2007 Added function to evaluate a boolean expression 
#              in the context of a classad.
#  15-Sep-2011 Added helper function to accumulate and return all the attribute 
#              names in a classad.
#
#  Description:
#  c-callable layer for handling Classad parse and unparse via the 'new'
#   ClassAd library.
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

#include <string.h>
#include "classad/classad_distribution.h"
#include "classad_binary_op_unwind.h"

#ifdef WANT_NAMESPACES
using namespace classad;
#endif

extern "C"
 {

#include <string.h>
#include "classad_c_helper.h"

  classad_context 
  classad_parse (char *s_in)
   {
    ClassAd *ad = NULL;
    ClassAdParser parser;
    ad = parser.ParseClassAd(s_in);
  
    // ad == NULL on error.
    return ((classad_context)ad);
   }

  char *
  classad_unparse (classad_context cad)
   {
    if (cad == NULL) return NULL;

    std::string res_s;
    char *res_c = NULL;

    ClassAd *ad = (ClassAd *)cad;
    ClassAdUnParser unparser;
    unparser.Unparse(res_s, ad);
  
    if (res_s.size() > 0)
     {
      res_c = strdup(res_s.c_str());
     }
    // res_c == NULL on error.
    return (res_c);
   }

  int
  classad_get_string_attribute (classad_context cad, const char *attribute_name, 
                                char *result, int l_result)
   {
    if (cad == NULL) return C_CLASSAD_INVALID_CONTEXT;

    ClassAd *ad = (ClassAd *)cad;

    Value v;
    ad->EvaluateAttr(attribute_name, v);

    if (v.IsStringValue( result, l_result ))
     {
      return C_CLASSAD_NO_ERROR;
     }
    
    return C_CLASSAD_VALUE_NOT_FOUND;
   }

  int
  classad_get_dstring_attribute (classad_context cad, const char *attribute_name, 
                                 char **result)
   {
    if (cad == NULL) return C_CLASSAD_INVALID_CONTEXT;

    ClassAd *ad = (ClassAd *)cad;
    if (result != NULL) *result = NULL; /* For those who don't check */
                                        /* the return code... */

    Value v;
    ad->EvaluateAttr(attribute_name, v);

    std::string res_str;

    if (v.IsStringValue( res_str ))
     {
      (*result) = strdup(res_str.c_str());
      if ((*result) == NULL) return C_CLASSAD_OUT_OF_MEMORY;
      return C_CLASSAD_NO_ERROR;
     }
    
    return C_CLASSAD_VALUE_NOT_FOUND;
   }

  void
  classad_free_string_list(char **strl)
   {
    if (strl != NULL)
     {
      char **str_val;
      for(str_val=strl; (*str_val) != NULL; str_val++) 
       {
        if ((*str_val)!=NULL) free(*str_val);
       }
      free(strl);
     }
   }

  int
  classad_get_string_list_attribute (classad_context cad, 
                                     const char *attribute_name, 
                                     char ***result)
   {
    if (cad == NULL) return C_CLASSAD_INVALID_CONTEXT;

    int n_results = 0;
    (*result) = (char **)malloc(sizeof(char **));
    if ((*result) == NULL) return C_CLASSAD_OUT_OF_MEMORY;

    (*result)[0] = NULL;

    ClassAd *ad = (ClassAd *)cad;

    Value vl;
    ad->EvaluateAttr(attribute_name, vl);

    const ExprList *et_result;
    if (vl.IsListValue(et_result)) 
     {
      std::vector<ExprTree*> ads;
      et_result->GetComponents(ads);
      // Get string values.
      for(std::vector<ExprTree*>::const_iterator it = ads.begin();
          it != ads.end(); ++it) 
       {
        if (dynamic_cast<Literal *>(*it) != nullptr) 
         {
          Value v;
          EvalState       state;
          state.SetScopes( ad );

          (*it)->Evaluate(state,v);

          std::string res_str;
          if (v.IsStringValue( res_str ))
           {
            // add string value to result, which is a NULL-terminated
            // string array.
            n_results++;
            (*result) = (char **)realloc(*result, (n_results+1)*sizeof(char *));
            if ((*result) == NULL) return C_CLASSAD_OUT_OF_MEMORY;
            (*result)[n_results-1] = strdup(res_str.c_str());
            (*result)[n_results] = NULL;
           }
         }
       }
      return C_CLASSAD_NO_ERROR;
     }
    
    // The result list needs to be freed on success only.
    classad_free_string_list(*result);
    (*result) = NULL;
    return C_CLASSAD_VALUE_NOT_FOUND;
   }

  int
  classad_get_int_attribute (classad_context cad, const char *attribute_name, 
                             int *result)
   {
    if (cad == NULL) return C_CLASSAD_INVALID_CONTEXT;

    ClassAd *ad = (ClassAd *)cad;

    Value v;
    ad->EvaluateAttr(attribute_name, v);

    if (v.IsIntegerValue( *result ))
     {
      return C_CLASSAD_NO_ERROR;
     }
    
    return C_CLASSAD_VALUE_NOT_FOUND;
   }

  int
  classad_get_bool_attribute (classad_context cad, const char *attribute_name, 
                              int *result)
   {
    if (cad == NULL) return C_CLASSAD_INVALID_CONTEXT;

    ClassAd *ad = (ClassAd *)cad;

    Value v;
    ad->EvaluateAttr(attribute_name, v);

    bool tmp_res;

    if (v.IsBooleanValue( tmp_res ))
     {
      if (tmp_res) *result = 1;
      else         *result = 0;
      return C_CLASSAD_NO_ERROR;
     }
    
    return C_CLASSAD_VALUE_NOT_FOUND;
   }

  int
  classad_put_string_attribute (classad_context *cad, const char *name, 
                                const char *value)
   {
    ClassAd *ad;
    if ((*cad) == NULL) 
     {
      ad = new ClassAd;
      (*cad) = (classad_context) ad;
     }
    else ad = (ClassAd *)(*cad);

    std::string str_val(value);

    if (ad->InsertAttr (name, str_val)) return C_CLASSAD_NO_ERROR; 
    else                                return C_CLASSAD_INSERT_FAILED;
   }

  int
  classad_put_string_list_attribute (classad_context *cad, 
                                     const char *name, 
                                     char **value)
   {
    if (value == NULL) return C_CLASSAD_INVALID_VALUE;

    ClassAd *ad;
    if ((*cad) == NULL) 
     {
      ad = new ClassAd;
      (*cad) = (classad_context) ad;
     }
    else ad = (ClassAd *)(*cad);

    char **str_val;

    std::vector<ExprTree*> et_ads;

    // Traverse NULL-terminated string array.
    for(str_val=value; (*str_val) != NULL; str_val++) 
     {
      Value v;
      v.SetStringValue(*str_val);
      et_ads.push_back(Literal::MakeLiteral(v));
     }

    ExprTree *et_value = ExprList::MakeExprList(et_ads);

    if (ad->Insert (name, et_value)) return C_CLASSAD_NO_ERROR;
    else                             return C_CLASSAD_INSERT_FAILED;
   }

  int
  classad_put_int_attribute (classad_context *cad, const char *name, 
                             int value)
   {
    ClassAd *ad;
    if ((*cad) == NULL) 
     {
      ad = new ClassAd;
      (*cad) = (classad_context) ad;
     }
    else ad = (ClassAd *)(*cad);

    if (ad->InsertAttr (name, value)) return C_CLASSAD_NO_ERROR; 
    else                              return C_CLASSAD_INSERT_FAILED;
   }

  int
  classad_put_bool_attribute (classad_context *cad, const char *name, 
                              int value)
   {
    ClassAd *ad;
    if ((*cad) == NULL) 
     {
      ad = new ClassAd;
      (*cad) = (classad_context) ad;
     }
    else ad = (ClassAd *)(*cad);

    bool tmp_value;

    if (value != 0) tmp_value = true;
    else            tmp_value = false;

    if (ad->InsertAttr (name, tmp_value)) return C_CLASSAD_NO_ERROR; 
    else                                  return C_CLASSAD_INSERT_FAILED;

   }

  void
  classad_free (classad_context cad)
   {
    ClassAd *ad;
    if (cad != NULL)
     {
      ad = (ClassAd *)cad;
      delete ad;
     }
   }

  void
  classad_dump (classad_context cad)
   {
    ClassAd *ad;
    if (cad != NULL)
     {
      ad = (ClassAd *)cad;
      ad->Puke();
     }
   }

  int
  unwind_attributes(classad_context cad, char *attribute_name, char ***results) 
   {
    if (cad == NULL) return C_CLASSAD_INVALID_CONTEXT;
    if ((results == NULL) || (attribute_name == NULL))
      return C_CLASSAD_INVALID_ARG;

    ClassAd *ad = (ClassAd *)cad;

    ExprTree *et;
    bool need_to_delete_et = false;

    et = ad->Lookup(attribute_name);
    if (et == NULL)
     {
      return C_CLASSAD_VALUE_NOT_FOUND;
     }

	if (dynamic_cast<Literal *>(et) != nullptr) 
     {
      // The attribute was probably stringified. Try to parse it.
      Value v;
      EvalState state;
      state.SetScopes( ad );

      et->Evaluate(state,v);
      std::string strres;

      if (v.IsStringValue( strres ))
       {
        ClassAdParser parser;
        et=NULL;
        parser.ParseExpression(strres,et);
        need_to_delete_et = true;
       }
     }

    BinaryOpUnwind res_unp; 
    std::string result;
    res_unp.Unparse(result, et);
    int n_results;
    if (*results == NULL)
     {
      n_results = 0;
      (*results) = (char **)malloc(sizeof(char **));
      if ((*results) == NULL) return C_CLASSAD_OUT_OF_MEMORY;
      (*results)[0] = NULL;
     }
    else
     {
      for (n_results = 0; (*results)[n_results] != NULL; n_results++) /*NOP*/ ;
     }

    std::vector<std::string>::const_iterator it;
    for (it = res_unp.m_unwind_output.begin(); 
         it != res_unp.m_unwind_output.end(); ++it)
     {
      n_results++;
      char **new_results;
      new_results = (char **)realloc(*results, (n_results+1)*sizeof(char *));
      if (new_results == NULL)
       {
        if (need_to_delete_et) delete et;
        return C_CLASSAD_OUT_OF_MEMORY;
       }
      (*results) = new_results;
      (*results)[n_results] = NULL;
      (*results)[n_results-1] = strdup(it->c_str());
      if (((*results)[n_results-1]) == NULL)
       {
        if (need_to_delete_et) delete et;
        return C_CLASSAD_OUT_OF_MEMORY;
       }
     }

    if (need_to_delete_et) delete et;
    return C_CLASSAD_NO_ERROR;
   }

  classad_expr_tree 
  classad_parse_expr(const char *s_ex)
   {
    ClassAdParser parser;
    ExprTree *et = parser.ParseExpression(s_ex);

    // et == NULL on error.
    return ((classad_expr_tree)et);
   }

  void 
  classad_free_tree(classad_expr_tree t_ex)
   {
    ExprTree *et = (ExprTree *)t_ex;
    if (et) delete et;
   }

  int
  classad_evaluate_boolean_expr(const char *s_in, const classad_expr_tree t_ex,
                                int *result)
   {
    ClassAd *ad;
    ClassAdParser parser;
    
    if (s_in == NULL || t_ex == NULL || result == NULL) 
      return C_CLASSAD_INVALID_ARG;

    ExprTree *et = (ExprTree *)t_ex;

    ad = parser.ParseClassAd(s_in);
    if (ad == NULL) return C_CLASSAD_PARSE_ERROR;

    int retcod = C_CLASSAD_NO_ERROR;

    Value v;
    et->SetParentScope(ad);
    ad->EvaluateExpr(et, v);
    et->SetParentScope(NULL);

    bool tmp_res;

    if (v.IsBooleanValue( tmp_res ))
     {
      if (tmp_res) *result = 1;
      else         *result = 0;
      retcod = C_CLASSAD_NO_ERROR;
     }
    else retcod = C_CLASSAD_INVALID_VALUE;

    delete ad;  

    return retcod;
   }

  int
  classad_get_attribute_names(classad_context cad, char ***results) 
   {
    if (cad == NULL) return C_CLASSAD_INVALID_CONTEXT;
    if (results == NULL)
      return C_CLASSAD_INVALID_ARG;

    ClassAd *ad = (ClassAd *)cad;

    int n_results = 0;

    ClassAd::const_iterator it;
    for (it = ad->begin(); it != ad->end(); ++it)
     {
      n_results++;
      char **new_results;
      new_results = (char **)realloc(*results, (n_results+1)*sizeof(char *));
      if (new_results == NULL)
       {
        return C_CLASSAD_OUT_OF_MEMORY;
       }
      (*results) = new_results;
      (*results)[n_results] = NULL;
      (*results)[n_results-1] = strdup(it->first.c_str());
      if (((*results)[n_results-1]) == NULL)
       {
        return C_CLASSAD_OUT_OF_MEMORY;
       }
     }

    return C_CLASSAD_NO_ERROR;
   }

  void
  classad_free_results(char **results) 
   {
    char **cur;
    if (results != NULL)
     {
      for(cur=results; (*cur)!=NULL; cur++)
       {
        free(*cur);
       }
      free(results);
     }
   }

 } // end of extern "C"
