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
} param_info_t_type_t;

// This struct is common to all params, int and double params
// will be followed by an int or double when default_valid is true,
// ranged params will have min and max values following that if range_valid is true.
typedef struct param_info_t_s {
    // The commented out things are structures defined in the param table but
    // not currently utilized by Condor.  They were removed to decrease overall
    // memory size of the param table.

	const char * name;
    //char const *  aliases;
	const char * str_val;
    //const char * version;
    //char const *  range;
    //const char * friendly_name;
    //const char * usage;
    //const char * url;
    //const char * tags;

    //int		id;

    param_info_t_type_t  	type;
    //int 	state;
    //int 	customization;
    //int 	reconfig;
    //int 	is_macro;
	int		default_valid;
	int 	range_valid;

} param_info_t;

struct param_info_str_t_s {
   param_info_t hdr;
//   const char * str_val;
};

struct param_info_str_ranged_t_s {
   param_info_t hdr;
   char * str_val;
   char * range;
};

struct param_info_int_t_s {
   param_info_t hdr;
   int int_val;
};

struct param_info_int_ranged_t_s {
   param_info_t hdr;
   int int_val;
   int int_min;
   int int_max;
};

struct param_info_dbl_t_s {
   param_info_t hdr;
   double dbl_val;
};

struct param_info_dbl_ranged_t_s {
   param_info_t hdr;
   double dbl_val;
   double dbl_min;
   double dbl_max;
};

typedef struct param_info_str_t_s param_info_PARAM_TYPE_STRING;
typedef struct param_info_int_t_s param_info_PARAM_TYPE_BOOL;
typedef struct param_info_int_t_s param_info_PARAM_TYPE_INT;
typedef struct param_info_dbl_t_s param_info_PARAM_TYPE_DOUBLE;
typedef struct param_info_str_t_s        param_info_PARAM_TYPE_STRING_ranged;
typedef struct param_info_int_ranged_t_s param_info_PARAM_TYPE_INT_ranged;
typedef struct param_info_dbl_ranged_t_s param_info_PARAM_TYPE_DOUBLE_ranged;

typedef union param_info_storage_u {
   struct param_info_t_s hdr;
   param_info_PARAM_TYPE_STRING type_string;
   param_info_PARAM_TYPE_BOOL type_bool;
   param_info_PARAM_TYPE_INT type_int;
   param_info_PARAM_TYPE_DOUBLE type_double;
   param_info_PARAM_TYPE_STRING_ranged type_string_ranged;
   param_info_PARAM_TYPE_INT_ranged type_int_ranged;
   param_info_PARAM_TYPE_DOUBLE_ranged type_double_ranged;
} param_info_storage_t;


#ifdef  __cplusplus
extern "C" {
#endif

	namespace condor_params {
		typedef struct key_value_pair key_value_pair;
		typedef struct key_table_pair key_table_pair;
	}
	typedef const struct condor_params::key_value_pair MACRO_DEF_ITEM;
	typedef const struct condor_params::key_table_pair MACRO_TABLE_PAIR;

	int param_info_init(const void ** pvdefaults);

	int param_default_integer(const char* param, const char * subsys, int* valid, int* is_long);
	int param_default_boolean(const char* param, const char * subsys, int* valid);
	double param_default_double(const char* param, const char * subsys, int* valid);
	long long param_default_long(const char* param, const char * subsys, int* valid);
	//returns pointer to internal object (or null), do not free
	const char* param_default_string(const char* param, const char * subsys);
	// param may be param or subsys.param, will return non-null only on exact name match
	const char* param_exact_default_string(const char* param);
	int param_default_get_id(const char*param);
	const char* param_default_name_by_id(int ix);
	const char* param_default_rawval_by_id(int ix);
	param_info_t_type_t param_default_type_by_id(int ix);
	bool param_default_ispath_by_id(int ix);
	MACRO_DEF_ITEM *param_subsys_default_lookup(const char *subsys, const char *name);
	MACRO_DEF_ITEM *param_default_lookup(const char *name);

	MACRO_TABLE_PAIR * param_meta_table(const char * meta);
	MACRO_DEF_ITEM * param_meta_table_lookup(MACRO_TABLE_PAIR *, const char * param);
	const char * param_meta_table_string(MACRO_TABLE_PAIR *, const char * param);
	int param_default_get_source_meta_id(const char * meta, const char * param);
	MACRO_DEF_ITEM * param_meta_source_by_id(int meta_id);


	// Returns -1 if param is not of the specified type.
	// Otherwise, returns 0 and sets min and max to the minimum and maximum
	// possible values.
	int param_range_integer(const char* param, int* min, int* max);
	int param_range_long(const char* param, long long* min, long long* max);
	int param_range_double(const char* param, double* min, double* max);
	
	// Iterate the list of parameter information.
	// See param_info_hash_iterate below.
	void iterate_params(int (*callPerElement)
			(const param_info_t* /*value*/, void* /*user data*/), void* user_data);

#ifdef  __cplusplus
} // extern "C"
#endif

///////////////////
// hash table stuff
///////////////////

// Picked a table size by looking for a prime number about equal to the
// half the occupancy number as of July 2012.  Because we're cramped for memory,
// we prefer hash collisions over memory usage.
#define PARAM_INFO_TABLE_SIZE	389

struct bucket_t {
	param_info_storage_t param;
	struct bucket_t* next;
};
typedef struct bucket_t bucket_t;

typedef bucket_t** param_info_hash_t;

void param_info_hash_insert(param_info_hash_t param_info, const param_info_storage_t* p);

//returns a pointer to an internal object, do not free the returned pointer
const param_info_t* param_info_hash_lookup(param_info_hash_t param_info, const char* param);

//must call this on a param_info_hash_t* to initialize it
void param_info_hash_create(param_info_hash_t* param_info);

///////////////////////////////
// stuff to make iterating work
///////////////////////////////

// The function is called on every element as the hash table is iterated.
// Continues as long as callPerElement returns 0, or until it iterates
// everything.  user_data is passed to every call of callPerElement.
void param_info_hash_iterate(param_info_hash_t param_info,
		int (*callPerElement) (const param_info_t* /*value*/, void* /*user_data*/),
		void* user_data);

// Dump the whole hash table.
void param_info_hash_dump(param_info_hash_t param_info);

// Print out information for one given value.  Typed to be used with
// param_info_hash_iterate.
int param_info_hash_dump_value(const param_info_t* param_value, void* unused);

// Optimize the memory layout of the hash table.
void param_info_hash_optimize(param_info_hash_t param_info);

#endif

