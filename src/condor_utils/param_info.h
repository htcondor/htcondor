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


#ifndef __PARAM_INFO_H__
#define __PARAM_INFO_H__

typedef enum param_info_t_type_e {
	PARAM_TYPE_STRING = 0,
	PARAM_TYPE_INT = 1,
	PARAM_TYPE_BOOL = 2,
	PARAM_TYPE_DOUBLE = 3,
	PARAM_TYPE_LONG = 4,
	PARAM_TYPE_KVP_TABLE = 14,
	PARAM_TYPE_KTP_TABLE = 15,
} param_info_t_type_t;

#define PARAM_DEFAULTS_SORTED

namespace condor_params {
	typedef struct key_value_pair key_value_pair;
	typedef struct key_table_pair key_table_pair;
	typedef struct ktp_value ktp_value;
}
typedef const struct condor_params::key_value_pair MACRO_DEF_ITEM;
typedef const struct condor_params::key_table_pair MACRO_TABLE_PAIR;
typedef const struct condor_params::ktp_value      MACRO_META_TABLES;

int param_info_init(const void ** pvdefaults);

int param_default_integer(const char* param, const char * subsys, int* valid, int* is_long, int* truncated);
int param_default_boolean(const char* param, const char * subsys, int* valid);
double param_default_double(const char* param, const char * subsys, int* valid);
long long param_default_long(const char* param, const char * subsys, int* valid);
//returns pointer to internal object (or null), do not free
const char* param_default_string(const char* param, const char * subsys);
// param may be param or subsys.param, will return non-null only on exact name match
const char* param_exact_default_string(const char* param);
int param_default_get_id(const char*param, const char* * pdot);
const char* param_default_name_by_id(int ix);
const char* param_default_rawval_by_id(int ix);
// sets on of imin, dmin, or lmin to point to the min value of the range and returns the type of the param
param_info_t_type_t param_default_range_by_id(int ix, const int *&imin, const double*&dmin, const long long*&lmin);
// this is in param_info_help.cpp to avoid pulling the help tables in unless this function is used.
int param_default_help_by_id(int ix, const char * & descrip, const char * & tags, const char * & used_for);
param_info_t_type_t param_default_type_by_id(int ix);
bool param_default_ispath_by_id(int ix);
MACRO_DEF_ITEM *param_subsys_default_lookup(const char *subsys, const char *name);
MACRO_DEF_ITEM *param_default_lookup(const char *name);
int param_get_subsys_table(const void* pvdefaults, const char* subsys, MACRO_DEF_ITEM** ppTable);

MACRO_TABLE_PAIR * param_meta_table(const char * meta, int * base_meta_id);
MACRO_TABLE_PAIR * param_meta_table(MACRO_META_TABLES & knobsets, const char * meta, int * base_meta_id);
MACRO_DEF_ITEM * param_meta_table_lookup(MACRO_TABLE_PAIR *, const char * param, int * meta_offset);
const char * param_meta_table_string(MACRO_TABLE_PAIR *, const char * param, int * meta_offset);
//int param_default_get_source_meta_id(const char * meta, const char * param);
const char * param_meta_value(const char * meta, const char * param, int * meta_id);
MACRO_DEF_ITEM * param_meta_source_by_id(int meta_id, MACRO_TABLE_PAIR ** ptable);


// Returns -1 if param is not of the specified type.
// Otherwise, returns 0 and sets min and max to the minimum and maximum
// possible values.
int param_range_integer(const char* param, int* min, int* max);
int param_range_long(const char* param, long long* min, long long* max);
int param_range_double(const char* param, double* min, double* max);


// binary search of an array of structures containing a member psz
// find the (case insensitive) matching element in the array
// and return a pointer to that element.
template <typename T>
const T * BinaryLookup (const T aTable[], int cElms, const char * key, int (*fncmp)(const char *, const char *))
{
	if (cElms <= 0)
		return NULL;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return NULL; // return null for "not found"

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = fncmp(aTable[ix].key, key);
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[ix];
	}
}

#endif

