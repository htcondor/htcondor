
#ifndef __PARAM_INFO_H__
#define __PARAM_INFO_H__

#include "condor_common.h"

typedef struct param_info_t_s {

	char*	name;
    char* 	aliases;
	union {
		int 	int_val;
		char* 	str_val;
		double  dbl_val;
	} default_val;
	char*   str_val;
	int		default_valid;
    char* 	range;
	union {
		int		int_min;
		double	dbl_min;
	} range_min;
	union {
		int		int_max;
		double	dbl_max;
	} range_max;
	int 	range_valid;
    int 	customization;
    int 	reconfig;
    int 	is_macro;
    char* 	version;
    int 	state;
    char*   friendly_name;
    int  	type;
    int		id;
    char*   usage;
    char*   url;
    char*   tags;

} param_info_t;

BEGIN_C_DECLS

	void param_info_insert(char* param, char* aliases, char* value, char* version,
						char* range, int state, int type, int is_macro, int reconfig,
						int customization, char* friendly_name, char* usage, char* url,
						char* tags);

	void param_info_init(void);

	int param_default_integer(const char* param, int* valid);
	int param_default_boolean(const char* param, int* valid);
	double param_default_double(const char* param, int* valid);
	//returns pointer to internal object (or null), do not free
	char* param_default_string(const char* param);

	// Returns -1 if param is not of the specified type.
	// Otherwise, returns 0 and sets min and max to the minimum and maximum
	// possible values.
	int param_range_integer(const char* param, int* min, int* max);
	int param_range_double(const char* param, double* min, double* max);
	
	// Iterate the list of parameter information.
	// See param_info_hash_iterate below.
	void iterate_params(int (*callPerElement)
			(param_info_t* /*value*/, void* /*user data*/), void* user_data);

END_C_DECLS


///////////////////
// hash table stuff
///////////////////

#define PARAM_INFO_TABLE_SIZE	2048

struct bucket_t {
	param_info_t* param;
	struct bucket_t* next;
};
typedef struct bucket_t bucket_t;

typedef bucket_t** param_info_hash_t;

void param_info_hash_insert(param_info_hash_t param_info, param_info_t* p);

//returns a pointer to an internal object, do not free the returned pointer
param_info_t* param_info_hash_lookup(param_info_hash_t param_info, const char* param);

//must call this on a param_info_hash_t* to initialize it
void param_info_hash_create(param_info_hash_t* param_info);

///////////////////////////////
// stuff to make iterating work
///////////////////////////////

// The function is called on every element as the hash table is iterated.
// Continues as long as callPerElement returns 0, or until it iterates
// everything.  user_data is passed to every call of callPerElement.
void param_info_hash_iterate(param_info_hash_t* param_info,
		int (*callPerElement) (param_info_t* /*value*/, void* /*user_data*/),
		void* user_data);

// Dump the whole hash table.
void param_info_hash_dump(param_info_hash_t* param_info);

// Print out information for one given value.  Typed to be used with
// param_info_hash_iterate.
int param_info_hash_dump_value(param_info_t* param_value, void* unused);

#endif

