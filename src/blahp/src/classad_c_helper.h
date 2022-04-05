/*
  File :     classad_c_helper.h


  Author :   Francesco Prelz ($Author: fprelz $)
  e-mail :   "francesco.prelz@mi.infn.it"

  Revision history :
   5-Apr-2004 Original release
  16-Apr-2004 Added string list parse and unparse.
   7-May-2004 Added classad_get_dstring_attribute prototype.
  19-Aug-2004 Added boolean attribute.
  30-Nov-2007 Added helper function to evaluate a boolean expression
              in a classad;
  15-Sep-2011 Added helper function to accumulate and return all the attribute 
              names in a classad.

  Description:
   Prototypes and typedefs for c-callable layer for handling Classad parse and 
   unparse via the 'new' ClassAd library.

  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 

    See http://www.eu-egee.org/partners/ for details on the copyright
    holders.  
  
    Licensed under the Apache License, Version 2.0 (the "License"); 
    you may not use this file except in compliance with the License. 
    You may obtain a copy of the License at 
  
        http://www.apache.org/licenses/LICENSE-2.0 
  
    Unless required by applicable law or agreed to in writing, software 
    distributed under the License is distributed on an "AS IS" BASIS, 
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
    See the License for the specific language governing permissions and 
    limitations under the License.

*/

#ifndef CLASSAD_C_HELPER_INCLUDED
#define CLASSAD_C_HELPER_INCLUDED

typedef void * classad_context;
typedef void * classad_expr_tree;

#define C_CLASSAD_NO_ERROR            0
#define C_CLASSAD_INVALID_CONTEXT    -1
#define C_CLASSAD_VALUE_NOT_FOUND    -2
#define C_CLASSAD_OUT_OF_MEMORY      -3
#define C_CLASSAD_INSERT_FAILED      -4
#define C_CLASSAD_INVALID_VALUE      -5
#define C_CLASSAD_INVALID_ARG        -6
#define C_CLASSAD_PARSE_ERROR        -7

classad_context classad_parse (char *s_in);
char * classad_unparse (classad_context cad);
int classad_get_string_attribute (classad_context cad, const char *attribute_name, 
                                  char *result, int l_result);
int classad_get_dstring_attribute (classad_context cad, const char *attribute_name,
                                   char **result);

int classad_get_string_list_attribute (classad_context cad, 
                                       const char *attribute_name, 
                                       char ***result);
int classad_get_int_attribute (classad_context cad, const char *attribute_name, 
                               int *result);
int classad_get_bool_attribute (classad_context cad, const char *attribute_name, 
                                int *result);
int classad_put_string_attribute (classad_context *cad, const char *name, 
                                  const char *value);
int classad_put_string_list_attribute (classad_context *cad, const char *name,
                                       char **value);
int classad_put_int_attribute (classad_context *cad, const char *name, 
                               int value);
int classad_put_bool_attribute (classad_context *cad, const char *name, 
                                int value);
void classad_free (classad_context cad);
void classad_free_string_list(char **strl);
void classad_dump (classad_context cad);
int unwind_attributes(classad_context cad, char *attribute_name, char ***results);
classad_expr_tree classad_parse_expr(const char *s_ex);
int classad_evaluate_boolean_expr(const char *s_in, const classad_expr_tree t_ex, int *result);
void classad_free_tree(classad_expr_tree t_ex);
int classad_get_attribute_names(classad_context cad, char ***results);
void classad_free_results(char **results);

#endif /* defined CLASSAD_C_HELPER_INCLUDED */
