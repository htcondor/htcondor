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


#ifndef __CLASSAD_CCLASSAD_H_
#define __CLASSAD_CCLASSAD_H_

/*
This is a C interface to the classad library.
This allows programs to use the ClassAd library in
a compiler-independent fashion with traditional constructs
such as integers and C strings.  No special compiler flags
or other packages are necessary to use this interface.
*/

#ifdef __cplusplus
namespace classad {
extern "C" {
#endif

struct classad;

/*
Create and delete ClassAds.
If str is null, the ad is empty.
If str is non-null, the ad is parsed from the given string.
Returns a new ClassAd or null if the input is invalid or memory is full.
*/

struct cclassad * cclassad_create( const char *str );
void cclassad_delete( struct cclassad *c );

/*
Display a ClassAd in its external representation.
Returns a string allocated with malloc().
The user is responsible for freeing it.
*/

char * cclassad_unparse( struct cclassad *c );
char * cclassad_unparse_xml( struct cclassad *c );

/*
Check to see if two ClassAds match.
Return true if their requirements expressions are
mutually satisfied.  Otherwise, return false.
*/

int cclassad_match( struct cclassad *a, struct cclassad *b );

/*
Four ways to insert elements into a classad.
In each case, "attr" names the attribute to be
inserted. In the "expr" form, the "value" must be a ClassAd
expression which is parsed and then inserted.
The remaining forms insert atomic types without parsing.
Returns true on success, false on failure.
*/

int cclassad_insert_expr( struct cclassad *c, const char *attr, const char *value );
int cclassad_insert_string( struct cclassad *c, const char *attr, const char *value );
int cclassad_insert_double( struct cclassad *c, const char *attr, double value );
int cclassad_insert_int( struct cclassad *c, const char *attr, int value ); 
int cclassad_insert_long( struct cclassad *c, const char *attr, long value );
int cclassad_insert_long_long( struct cclassad *c, const char *attr, long long value );
int cclassad_insert_bool( struct cclassad *c, const char *attr, int value );

/*
Remove the named element from the ClassAd.
Returns true if the attribute existed, false otherwise.
*/

int cclassad_remove( struct cclassad *c, const char *attr );

/*
Four ways to evaluate the contents of a classad.
In each case, "expr" is an expression to be parsed and evaluted.
In the "to_expr" form, the result is given as an expression in
external form allocated with malloc().  The caller must free() it.
In the remaining forms, the results are given as atomic values
that need not be parsed.  Returns true on success, false on failure.
*/

int cclassad_evaluate_to_expr( struct cclassad *c, const char *expr, char **result );
int cclassad_evaluate_to_string( struct cclassad *c, const char *expr, char **result );
int cclassad_evaluate_to_double( struct cclassad *c, const char *expr, double *result );
int cclassad_evaluate_to_int( struct cclassad *c, const char *expr, int *result );
int cclassad_evaluate_to_long( struct cclassad *c, const char *expr, long *result );
int cclassad_evaluate_to_long_long( struct cclassad *c, const char *expr, long long *result );
int cclassad_evaluate_to_bool( struct cclassad *c, const char *expr, int *result );

#ifdef __cplusplus
} // extern "C"
} // namespace classad
#endif

#endif
