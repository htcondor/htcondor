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


#include "condor_common.h"
#include "condor_debug.h"
#include "param_info.h"
#ifdef HAVE_PCRE_PCRE_H
#  include "pcre/pcre.h"
#else
#  include "pcre.h"
#endif

#define CUSTOMIZATION_SELDOM 	0

#define RECONFIG_NORMAL 		0

#define STATE_DEFAULT			0
#define STATE_AUTODEFAULT		1
#define STATE_USER				2
#define STATE_RUNTIME			3

//param_info_hash_t = bucket_t**
//bucket_t** param_info;

#define PARAM_DECLARE_TABLES 1 // so param_info_table will give us the table declarations.
#include "param_info_tables.h"
int param_info_init(const void ** pvdefaults)
{
	*pvdefaults = condor_params::defaults;
	return condor_params::defaults_count;
}

#ifdef PARAM_DEFAULTS_SORTED

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

typedef const struct condor_params::key_value_pair param_table_entry_t;
const param_table_entry_t * param_generic_default_lookup(const char * param)
{
	const param_table_entry_t* found = NULL;
	found = BinaryLookup<param_table_entry_t>(
		condor_params::defaults,
		condor_params::defaults_count,
		param, strcasecmp);
	return found;
}

// case insensitive compare of two strings up to the first "." or \0
//
int ComparePrefixBeforeDot(const char * p1, const char * p2)
{
	for (;*p1 || *p2; ++p1, ++p2) {
		int ch1 = *p1, ch2 = *p2;
		if (ch1 == '.') ch1 = 0; else if (ch1 >= 'a') ch1 &= ~0x20; // change lower to upper
		if (ch2 == '.') ch2 = 0; else if (ch2 >= 'a') ch2 &= ~0x20; // this is cheap, but doesn't work for {|}~
		int diff = ch1 - ch2;
		if (diff) return diff;
		if ( ! ch1) break;
	}
	return 0;
}

// lookup param in in the subsys table, then in the default table(s).
// to simplify use by the caller only text up to the first "." in subsys
// is used for the lookup in the subsystems table. this allows one to pass in "MASTER.ATTRIBUTE"
// as the subsys value in order to locate the master table.
//
typedef const struct condor_params::key_value_pair MACRO_DEF_ITEM;
typedef const struct condor_params::key_table_pair MACRO_TABLE_PAIR;
MACRO_DEF_ITEM * param_subsys_default_lookup(const char * subsys, const char * param)
{
	MACRO_TABLE_PAIR* subtab = NULL;
	subtab = BinaryLookup<MACRO_TABLE_PAIR>(
		condor_params::subsystems,
		condor_params::subsystems_count,
		subsys, ComparePrefixBeforeDot);

	if (subtab) {
		return BinaryLookup<MACRO_DEF_ITEM>(subtab->aTable, subtab->cElms, param, strcasecmp);
	}
	return NULL;
}

MACRO_DEF_ITEM * param_default_lookup(const char * param, const char * subsys)
{
	if (subsys) {
		MACRO_DEF_ITEM * p = param_subsys_default_lookup(subsys, param);
		if (p) return p;
		// fall through to do generic lookup.
	}
	return param_generic_default_lookup(param);
}

// lookup a knob by metaname and by knobname.
//
int param_default_get_source_meta_id(const char * meta, const char * param)
{
	std::string fullname("$");
	fullname += meta;
	fullname += ":";
	fullname += param;

	MACRO_DEF_ITEM * p = BinaryLookup<MACRO_DEF_ITEM>(
		condor_params::metaknobsources,
		condor_params::metaknobsources_count,
		fullname.c_str(), strcasecmp);
	if (p) {
		return (int)(p - condor_params::metaknobsources);
	}
	return -1;
}

MACRO_DEF_ITEM * param_meta_source_by_id(int meta_id)
{
	if (meta_id < 0 || meta_id >= condor_params::metaknobsources_count)
		return NULL;
	return &condor_params::metaknobsources[meta_id];
}

// lookup a knob by metaname and by knobname.
//
MACRO_TABLE_PAIR* param_meta_table(const char * meta)
{
	return BinaryLookup<MACRO_TABLE_PAIR>(
		condor_params::metaknobsets,
		condor_params::metaknobsets_count,
		meta, ComparePrefixBeforeDot);
}

// lookup a param in the metatable
MACRO_DEF_ITEM * param_meta_table_lookup(MACRO_TABLE_PAIR* table, const char * param)
{
	if (table) {
		return BinaryLookup<MACRO_DEF_ITEM>(table->aTable, table->cElms, param, strcasecmp);
	}
	return NULL;
}

const char * param_meta_table_string(MACRO_TABLE_PAIR * table, const char * param)
{
	if (table) {
		MACRO_DEF_ITEM * p = BinaryLookup<MACRO_DEF_ITEM>(table->aTable, table->cElms, param, strcasecmp);
		if (p && p->def)
			return p->def->psz;
	}
	return NULL;
}

// this function can be passed either "ATTRIB" or "SUBSYS.ATTRIB"
// it will search for the first "." and if there is one it will lookup the param
// in the SUBSYS table, and in the generic table.  if not it will look only in the
// generic table
//
const param_table_entry_t * param_default_lookup(const char * param)
{
	const char * pdot = strchr(param, '.');
	if (pdot) {
		const param_table_entry_t * p = param_subsys_default_lookup(param, pdot+1);
		if (p) return p;
		// fall through to do generic lookup.
	}

	return param_generic_default_lookup(param);
}


int param_entry_get_type(const param_table_entry_t * p) {
	if ( ! p || ! p->def)
		return -1;
	if ( ! p->def->psz)
		return PARAM_TYPE_STRING;
	return reinterpret_cast<const condor_params::string_value *>(p->def)->flags & condor_params::PARAM_FLAGS_TYPE_MASK;
}

int param_entry_get_type(const param_table_entry_t * p, bool & ranged) {
	ranged = false;
	if ( ! p || ! p->def)
		return -1;
	if ( ! p->def->psz)
		return PARAM_TYPE_STRING;
	int flags = reinterpret_cast<const condor_params::string_value *>(p->def)->flags;
	ranged = (flags & condor_params::PARAM_FLAGS_RANGED) != 0;
	return (flags & condor_params::PARAM_FLAGS_TYPE_MASK);
}

int param_default_get_id(const char*param)
{
	int ix = -1;
	const param_table_entry_t* found = param_generic_default_lookup(param);
	if ( ! found) {
		const char * pdot = strchr(param, '.');
		if (pdot) {
			found = param_generic_default_lookup(pdot+1);
		}
	}
	if (found) ix = (int)(found - condor_params::defaults);
	return ix;
}

const char* param_default_name_by_id(int ix)
{
	if (ix >= 0 && ix < condor_params::defaults_count) {
		return condor_params::defaults[ix].key;
	}
	return NULL;
}

const char* param_default_rawval_by_id(int ix)
{
	if (ix >= 0 && ix < condor_params::defaults_count) {
		const param_table_entry_t* p = &condor_params::defaults[ix];
		if (p && p->def) {
			return p->def->psz;
		}
	}
	return NULL;
}

param_info_t_type_t param_default_type_by_id(int ix)
{
	if (ix >= 0 && ix < condor_params::defaults_count) {
		const param_table_entry_t* p = &condor_params::defaults[ix];
		if (p && p->def) {
			return (param_info_t_type_t)param_entry_get_type(p);
		}
	}
	return PARAM_TYPE_STRING;
}

bool param_default_ispath_by_id(int ix)
{
	if (ix >= 0 && ix < condor_params::defaults_count) {
		const param_table_entry_t* p = &condor_params::defaults[ix];
		if (p && p->def) {
			int flags = reinterpret_cast<const condor_params::string_value *>(p->def)->flags;
			return (flags & condor_params::PARAM_FLAGS_PATH) != 0;
		}
	}
	return false;
}

#endif // PARAM_DEFAULTS_SORTED

const char*
param_default_string(const char* param, const char * subsys)
{
	const param_table_entry_t* p = param_default_lookup(param, subsys);
	if (p && p->def) {
		return p->def->psz;
	}
	return NULL;
}

const char*
param_exact_default_string(const char* param)
{
	const param_table_entry_t* p = NULL;
	const char * pdot = strchr(param, '.');
	if (pdot) {
		p = param_subsys_default_lookup(param, pdot+1);
	} else {
		p = param_generic_default_lookup(param);
	}
	if (p && p->def) {
		return p->def->psz;
	}
	return NULL;
}

int
param_default_integer(const char* param, const char* subsys, int* valid, int* is_long) {
	int ret = 0;
#ifdef PARAM_DEFAULTS_SORTED
	if (valid) *valid = false;
	if (is_long) *is_long = false;
	const param_table_entry_t* p = param_default_lookup(param, subsys);
	if (p && p->def) {
		int type = param_entry_get_type(p);
		switch (type) {
			case PARAM_TYPE_INT:
				ret = reinterpret_cast<const condor_params::int_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
			case PARAM_TYPE_BOOL:
				ret = reinterpret_cast<const condor_params::bool_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
			case PARAM_TYPE_LONG:
				{
				long long tmp = reinterpret_cast<const condor_params::long_value *>(p->def)->val;
				ret = (int)tmp;
				if (ret != tmp) {
					if (tmp > INT_MAX) ret = INT_MAX;
					if (tmp < INT_MIN) ret = INT_MIN;
				};
				if (valid) *valid = true;
				if (is_long) *is_long = true;
				}
				break;
		}
	}
#else
	param_info_init();

	const param_info_t* p = param_info_hash_lookup(param_info, param);

	if (p && (p->type == PARAM_TYPE_INT || p->type == PARAM_TYPE_BOOL)) {
        *valid = p->default_valid;
        if (*valid)
            ret = reinterpret_cast<const param_info_PARAM_TYPE_INT*>(p)->int_val;
	} else {
		*valid = 0;
	}
#endif
	return ret;
}

int
param_default_boolean(const char* param, const char* subsys, int* valid) {
	return (param_default_long(param, subsys, valid) != 0);
}

long long
param_default_long(const char* param, const char* subsys, int* valid) {
	int ret = 0;
#ifdef PARAM_DEFAULTS_SORTED
	if (valid) *valid = false;
	const param_table_entry_t* p = param_default_lookup(param, subsys);
	if (p && p->def) {
		int type = param_entry_get_type(p);
		switch (type) {
			case PARAM_TYPE_INT:
				ret = reinterpret_cast<const condor_params::int_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
			case PARAM_TYPE_BOOL:
				ret = reinterpret_cast<const condor_params::bool_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
			case PARAM_TYPE_LONG:
				ret = reinterpret_cast<const condor_params::long_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
		}
	}
#else
	param_info_init();

	const param_info_t* p = param_info_hash_lookup(param_info, param);

	if (p && (p->type == PARAM_TYPE_INT || p->type == PARAM_TYPE_BOOL)) {
        *valid = p->default_valid;
        if (*valid)
            ret = reinterpret_cast<const param_info_PARAM_TYPE_INT*>(p)->int_val;
	} else {
		*valid = 0;
	}
#endif
	return ret;
}

double
param_default_double(const char* param, const char * subsys, int* valid) {
	double ret = 0.0;

#ifdef PARAM_DEFAULTS_SORTED
	const param_table_entry_t* p = param_default_lookup(param, subsys);
	if (valid) *valid = false;
	if (p && p->def) {
		int type = param_entry_get_type(p);
		switch (type) {
			case PARAM_TYPE_DOUBLE:
				ret = reinterpret_cast<const condor_params::double_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
			case PARAM_TYPE_INT:
				ret = reinterpret_cast<const condor_params::int_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
			case PARAM_TYPE_BOOL:
				ret = reinterpret_cast<const condor_params::bool_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
			case PARAM_TYPE_LONG:
				ret = reinterpret_cast<const condor_params::long_value *>(p->def)->val;
				if (valid) *valid = true;
				break;
		}
	}
#else
	param_info_init();

	const param_info_t* p = param_info_hash_lookup(param_info, param);

	if (p && (p->type == PARAM_TYPE_DOUBLE)) {
		*valid = p->default_valid;
        if (*valid)
		    ret = reinterpret_cast<const param_info_PARAM_TYPE_DOUBLE*>(p)->dbl_val;
	} else {
		*valid = 0;
	}
#endif
	return ret;
}

int
param_range_long(const char* param, long long* min, long long* max) {

#ifdef PARAM_DEFAULTS_SORTED
	int ret = -1; // not ranged.
	const param_table_entry_t* p = param_default_lookup(param);
	if (p && p->def) {
		bool ranged = false;
		int type = param_entry_get_type(p, ranged);
		switch (type) {
			case PARAM_TYPE_INT:
				if (ranged) {
					*min = reinterpret_cast<const condor_params::ranged_int_value*>(p->def)->min;
					*max = reinterpret_cast<const condor_params::ranged_int_value*>(p->def)->max;
					ret = 0;
				} else {
					*min = INT_MIN;
					*max = INT_MAX;
					ret = 0;
				}
				break;

			case PARAM_TYPE_LONG:
				if (ranged) {
					*min = reinterpret_cast<const condor_params::ranged_long_value*>(p->def)->min;
					*max = reinterpret_cast<const condor_params::ranged_long_value*>(p->def)->max;
					ret = 0;
				} else {
					*min = LLONG_MIN;
					*max = LLONG_MAX;
					ret = 0;
				}
				break;
		}
	}
	return ret;
#else
	PRAGMA_REMIND("write this!")
	*min = LLONG_MIN;
	*max = LLONG_MAX;
	ret = 0;
#endif
}

int
param_range_integer(const char* param, int* min, int* max) {

#ifdef PARAM_DEFAULTS_SORTED
	int ret = -1; // not ranged.
	const param_table_entry_t* p = param_default_lookup(param);
	if (p && p->def) {
		bool ranged = false;
		int type = param_entry_get_type(p, ranged);
		switch (type) {
			case PARAM_TYPE_INT:
				if (ranged) {
					*min = reinterpret_cast<const condor_params::ranged_int_value*>(p->def)->min;
					*max = reinterpret_cast<const condor_params::ranged_int_value*>(p->def)->max;
					ret = 0;
				} else {
					*min = INT_MIN;
					*max = INT_MAX;
					ret = 0;
				}
				break;

			case PARAM_TYPE_LONG:
				if (ranged) {
					*min = MAX(INT_MIN, reinterpret_cast<const condor_params::ranged_long_value*>(p->def)->min);
					*max = MIN(INT_MAX, reinterpret_cast<const condor_params::ranged_long_value*>(p->def)->max);
					ret = 0;
				} else {
					*min = INT_MIN;
					*max = INT_MAX;
					ret = 0;
				}
				break;
		}
	}
	return ret;
#else

	const param_info_t* p = param_info_hash_lookup(param_info, param);

	if (p) {
		if (p->type != PARAM_TYPE_INT) {
			return -1;
		}
        if ( ! p->range_valid) {
            *min = INT_MIN;
            *max = INT_MAX;
        } else {
		    *min = reinterpret_cast<const param_info_PARAM_TYPE_INT_ranged*>(p)->int_min;
		    *max = reinterpret_cast<const param_info_PARAM_TYPE_INT_ranged*>(p)->int_max;
        }
	} else {
		/* If the integer isn't known about, then don't assume a range for it */
/*		EXCEPT("integer range for param '%s' not found", param);*/
		return -1;
	}
	return 0;
#endif
}

int
param_range_double(const char* param, double* min, double* max) {

#ifdef PARAM_DEFAULTS_SORTED
	int ret = -1; // not ranged.
	const param_table_entry_t* p = param_default_lookup(param);
	if (p && p->def) {
		bool ranged = false;
		int type = param_entry_get_type(p, ranged);
		switch (type) {
			case PARAM_TYPE_DOUBLE:
				if (ranged) {
					*min = reinterpret_cast<const condor_params::ranged_double_value*>(p->def)->min;
					*max = reinterpret_cast<const condor_params::ranged_double_value*>(p->def)->max;
					ret = 0;
				} else {
					*min = DBL_MIN;
					*max = DBL_MAX;
					ret = 0;
				}
				break;
		}
	}
	return ret;
#else

	const param_info_t* p = param_info_hash_lookup(param_info, param);

	if (p) {
		if(p->type != PARAM_TYPE_DOUBLE) {
			return -1;
		}
        if ( ! p->range_valid) {
            *min = DBL_MIN;
            *max = DBL_MAX;
        } else {
		    *min = reinterpret_cast<const param_info_PARAM_TYPE_DOUBLE_ranged*>(p)->dbl_min;
		    *max = reinterpret_cast<const param_info_PARAM_TYPE_DOUBLE_ranged*>(p)->dbl_max;
        }
	} else {
		/* If the double isn't known about, then don't assume a range for it */
/*		EXCEPT("double range for param '%s' not found", param);*/
		return -1;
	}
	return 0;
#endif
}

#if 0
/* XXX This function probably needs a lot of work. */
static void
compute_range(const char* range, char** range_start, char** range_end) {

	const char * c1 = NULL, * c2 = NULL, * c3 = NULL;

	for (c1=range; isspace(*c1); c1++);				//skip leading whitespace

	for (c2=c1; *c2 && *c2!=','; c2++);				//find the first comma or null terminator

	// if the same or they match the generalized anything operator, there is no range
	if (c1==c2 || (strcmp(c1, ".*") == 0)) {
		*range_start = (char *)malloc(sizeof(char));
		**range_start = '\0';
		*range_end = (char*)malloc(sizeof(char));
		**range_end = '\0';
	} else {

		//find range_start
		for (c3=c2-1; isspace(*c3); c3--);			//reverse over trailing whitespace
		*range_start = (char *)calloc((c3-c1+2), sizeof(char));
		strncat(*range_start, c1, c3-c1+1);

		//find range_end
		for (c1=c2; *c1; c1++);						//find end of uppper limit, stopping at null terminator
		for (c3=c1-1; isspace(*c3); c3--);			//reverse over trailing whitespace
		for (c2++; c2<=c1 && isspace(*c2); c2++);	//skip leading whitespace
		*range_end = (char *)calloc((c3-c2+2), sizeof(char));
		strncat(*range_end, c2, c3-c2+1);

	}
}

static int
validate_integer_range_lower_bound(const char* range_start, int* min) {

	char* end_ptr;
	int valid;

	if (*range_start) {
		*min = strtol(range_start,&end_ptr,10);
		//if it's an empty string, it's not valid
		if (end_ptr != range_start) {
			//if rest of string is anything but whitespace it's not valid
			for ( ; isspace(*end_ptr); end_ptr++);
			valid = (*end_ptr == '\0');
		} else {
			valid = 0;
		}
	} else {
		valid = 0;
	}

	if (!valid) {
		//if not valid assume no bound
		*min = INT_MIN;
	}

	return valid;
}

static int
validate_integer_range_upper_bound(const char* range_end, int* max) {

	char* end_ptr;
	int valid;

	if (*range_end) {
		*max = strtol(range_end,&end_ptr,10);
		//if it's an empty string, it's not valid
		if (end_ptr != range_end) {
			//if rest of string is anything but whitespace it's not valid
			for ( ; isspace(*end_ptr); end_ptr++);
			valid = (*end_ptr == '\0');
		} else {
			valid = 0;
		}
	} else {
		valid = 0;
	}

	if (!valid) {
		//if not valid assume no bound
		*max = INT_MAX;
	}

	return valid;
}

static int
validate_double_range_lower_bound(const char* range_start, double* min) {

	char* end_ptr;
	int valid;

	if (*range_start) {
		*min = strtol(range_start,&end_ptr,10);
		//if it's an empty string, it's not valid
		if (end_ptr != range_start) {
			//if rest of string is anything but whitespace it's not valid
			for ( ; isspace(*end_ptr); end_ptr++);
			valid = (*end_ptr == '\0');
		} else {
			valid = 0;
		}
	} else {
		valid = 0;
	}

	if (!valid) {
		//if not valid assume no bound
		*min = DBL_MIN;
	}

	return valid;
}

static int
validate_double_range_upper_bound(const char* range_end, double* max) {

	char* end_ptr;
	int valid;

	if (*range_end) {
		*max = strtod(range_end,&end_ptr);
		//if it's an empty string, it's not valid
		if (end_ptr != range_end) {
			//if rest of string is anything but whitespace it's not valid
			for ( ; isspace(*end_ptr); end_ptr++);
			valid = (*end_ptr == '\0');
		} else {
			valid = 0;
		}
	} else {
		valid = 0;
	}

	if (!valid) {
		//if not valid assume no bound
		*max = DBL_MAX;
	}

	return valid;
}

static int
validate_regex(const char* pattern, const char* subject) {

	pcre* re;
	const char* err;
	int err_index;
    int group_count;
	int oveccount;
	int* ovector;
	int matches;
	int i;
	int is_valid = 0;
	int subject_len;

	//compile the regex with default options
	re = pcre_compile(pattern, 0, &err, &err_index, NULL);

	//figure out how many groups there are so we can size ovector appropriately
	pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &group_count);
	oveccount = 2 * (group_count + 1); // +1 for the string itself
	ovector = (int *)malloc(oveccount * sizeof(int));

	if (!ovector) {
		EXCEPT("unable to allocated memory for regex group info in validate_regex");
	}

	subject_len = strlen(subject);

	//run the regex
	matches = pcre_exec(re, NULL, subject, subject_len, 0, 0, ovector, oveccount);

	//make sure that one of the matches is the whole string
	for (i=0; i<matches; i++) {
		if (ovector[i*2] == 0 && ovector[i*2+1] == subject_len) {
			is_valid = 1;
			break;
		}
	}

	free(ovector);

	pcre_free(re);

	return is_valid;
}
#endif

void
iterate_params(int (*callPerElement)
				(const param_info_t* /*value*/, void* /*user data*/),
				void* user_data) {
#ifdef PARAM_DEFAULTS_SORTED
	const param_table_entry_t* table = reinterpret_cast<const param_table_entry_t*>(&condor_params::defaults);
	for (int ii = 0; ii < condor_params::defaults_count; ++ii) {
		param_info_t info = {table[ii].key, NULL, PARAM_TYPE_STRING, 0, 0};
		if (table[ii].def) {
			info.str_val = table[ii].def->psz;
			info.default_valid = true;
			int type = param_entry_get_type(&table[ii]);
			if (type >= 0) info.type = (param_info_t_type_t)type;
		}
		if (callPerElement(&info, user_data))
			break;
	}
#else
	param_info_hash_iterate(param_info, callPerElement, user_data);
#endif
}

