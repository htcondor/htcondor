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

// parse the range and set range_start and range_end to pointers to strings
// containing either the upper/lower bound of the range if valid or an empty
// string if not valid
static void compute_range(const char* range, char** range_start,
	char** range_end);

// take a string representation of the upper/lower limit and a pointer to an
// int/double to store the value extracted from the string or the min/max for
// int/double assumes no upper/lower bound if the string is empty return 1 if
// string represents a valid bound, 0 otherwise
static int validate_integer_range_upper_bound(const char* range_end,
	int* max);
static int validate_integer_range_lower_bound(const char* range_start,
	int* min);
static int validate_double_range_upper_bound(const char* range_end,
	double* max);
static int validate_double_range_lower_bound(const char* range_start,
	double* min);

// Ensure the subject matches the regex.
static int validate_regex(char* pattern, char* subject);

#define CUSTOMIZATION_SELDOM 	0

#define RECONFIG_NORMAL 		0

#define STATE_DEFAULT			0
#define STATE_AUTODEFAULT		1
#define STATE_USER				2
#define STATE_RUNTIME			3

//allocate and copy src to dest
#define CPY_STR(dst, src) \
if (src) { \
	dst = malloc(strlen(src)+1); \
	strcpy(dst, src); \
} else { \
	dst = NULL;  \
}

//param_info_hash_t = bucket_t**
bucket_t** param_info;

//static int num_entries;

void
param_info_init() 
{
	static int done_once = 0;

	// guard against multiple initializations of the default table.
	if (done_once == 1) {
		return;
	}

	// if I get here, I've done this task ONCE.
	done_once = 1;

	param_info_hash_create(&param_info);

#include "param_info_init.c"

}

void
param_info_insert(char* param,
				  char* aliases,
				  char* value,
				  char* version,
				  char* range,
				  int   state,
				  int	type,
				  int   is_macro,
				  int   reconfig,
				  int   customization,
				  char* friendly_name,
				  char* usage,
				  char* url,
				  char* tags)
{
	param_info_t* p;
	char* range_start;
	char* range_end;
	
	if (!param) {
		EXCEPT("param passed to param_info_insert was NULL");
	}

	p = malloc(sizeof(param_info_t));

	CPY_STR(p->name, param);
	CPY_STR(p->aliases, aliases);

	if (!value) {
		EXCEPT("value passed to param_info_insert was NULL");
	}
	if (!range) {
		EXCEPT("range passed to param_info_insert was NULL");
	}

	CPY_STR(p->str_val, value);

	p->type = type;
	p->range_valid = 1;
	p->default_valid = 1;
	switch (type) {

		case PARAM_TYPE_INT:

			p->default_val.int_val = strtol(value, NULL, 10);
			compute_range(range, &range_start, &range_end);
			if (!validate_integer_range_lower_bound(range_start, &(p->range_min.int_min))) {
/*				dprintf(D_ALWAYS, "invalid lower limit of range for '%s', assuming no lower bound\n", param);*/
				p->range_valid = 0;
			} else {
				if (p->default_val.int_val < p->range_min.int_min) {
					p->default_valid = 0;
					p->default_val.int_val = p->range_min.int_min;
/*					dprintf(D_ALWAYS, "default value for '%s': %d less than lower limit of: %d\n", param, p->default_val.int_val, p->range_min.int_min);*/
				}
			}
			if (!validate_integer_range_upper_bound(range_end, &(p->range_max.int_max))) {
/*				dprintf(D_ALWAYS, "invalid upper limit of range for '%s', assuming no uppper bound\n", param);*/
				p->range_valid = 0;
			} else {
				if (p->default_val.int_val > p->range_max.int_max) {
					p->default_valid = 0;
					p->default_val.int_val = p->range_max.int_max;
/*					dprintf(D_ALWAYS, "default value for '%s': %d greater than upper limit of: %d\n", param, p->default_val.int_val, p->range_max.int_max);*/
				}
			}

			free(range_start);
			free(range_end);

			break;

		case PARAM_TYPE_BOOL:

			p->default_val.int_val = strtol(value, NULL, 10) != 0;

			break;

		case PARAM_TYPE_DOUBLE:

			p->default_val.dbl_val = strtod(value, NULL);

			compute_range(range, &range_start, &range_end);
			if (!validate_double_range_lower_bound(range_start, &(p->range_min.dbl_min))) {
/*				dprintf(D_ALWAYS, "invalid lower limit of range for '%s', assuming no lower bound\n", param);*/
				p->range_valid = 0;
			} else {
				if (p->default_val.dbl_val < p->range_min.dbl_min) {
					p->default_valid = 0;
					p->default_val.dbl_val = p->range_min.dbl_min;
/*					dprintf(D_ALWAYS, "default value for '%s': %f less than lower limit of: %f\n", param, p->default_val.dbl_val, p->range_min.dbl_min);*/
				}
			}
			if (!validate_double_range_upper_bound(range_end, &(p->range_max.dbl_max))) {
/*				dprintf(D_ALWAYS, "invalid upper limit of range for '%s', assuming no uppper bound\n", param);*/
				p->range_valid = 0;
			} else {
				if (p->default_val.dbl_val > p->range_max.dbl_max) {
					p->default_valid = 0;
					p->default_val.dbl_val = p->range_max.dbl_max;
/*					dprintf(D_ALWAYS, "default value for '%s': %f greater than upper limit of: %f\n", param, p->default_val.dbl_val, p->range_max.dbl_max);*/
				}
			}

			free(range_start);
			free(range_end);

			break;

		default:

/*			dprintf(D_ALWAYS, "Invalid type specified for parameter '%s', defaulting to string\n", param);*/

		case PARAM_TYPE_STRING:

			if (validate_regex(range, value)) {
				p->default_val.str_val = p->str_val;
			} else {
/*				dprintf(D_ALWAYS, "invalid default value: '%s', not in range '%s' for parameter '%s'\n", value, range, param);*/
				p->default_valid = 0;
			}

			break;

	}

	p->state = state;
	p->is_macro = is_macro;
	p->reconfig = reconfig;
	p->customization = customization;
	CPY_STR(p->version, version);
	CPY_STR(p->friendly_name, friendly_name);
	CPY_STR(p->usage, usage);
	CPY_STR(p->url, url);
	CPY_STR(p->tags, tags);

	param_info_hash_insert(param_info, p);
}


char*
param_default_string(const char* param)
{
	param_info_t *p;
	char* ret = NULL;

	param_info_init();
	p = param_info_hash_lookup(param_info, param);

	// Don't check the type here, since this is used in param and is used
	// to look up values for all types.
	if (p && p->default_valid) {
		ret = p->str_val;
	}

	return ret;
}

int
param_default_integer(const char* param, int* valid) {
	param_info_t* p;
	int ret = 0;

	param_info_init();

	p = param_info_hash_lookup(param_info, param);

	if (p) {
		ret = p->default_val.int_val;
		*valid = p->default_valid && (p->type == PARAM_TYPE_INT || p->type == PARAM_TYPE_BOOL);
	} else {
		*valid = 0;
	}

	return ret;
}

int
param_default_boolean(const char* param, int* valid) {
	return (param_default_integer(param, valid) != 0);
}

double
param_default_double(const char* param, int* valid) {


	param_info_t* p;
	double ret = 0.0;

	param_info_init();

	p = param_info_hash_lookup(param_info, param);

	if (p) {
		ret = p->default_val.dbl_val;
		*valid = p->default_valid && (p->type == PARAM_TYPE_DOUBLE);
	} else {
		*valid = 0;
	}

	return ret;
}

int
param_range_integer(const char* param, int* min, int* max) {

	param_info_t* p;

	p = param_info_hash_lookup(param_info, param);

	if (p) {
		if(p->type != PARAM_TYPE_INT) {
			return -1;
		}
		*min = p->range_min.int_min;
		*max = p->range_max.int_max;
	} else {
		/* If the integer isn't known about, then don't assume a range for it */
/*		EXCEPT("integer range for param '%s' not found", param);*/
		return -1;
	}
	return 0;
}

int
param_range_double(const char* param, double* min, double* max) {

	param_info_t* p;

	p = param_info_hash_lookup(param_info, param);

	if (p) {
		if(p->type != PARAM_TYPE_DOUBLE) {
			return -1;
		}
		*min = p->range_min.dbl_min;
		*max = p->range_max.dbl_max;
	} else {
		/* If the double isn't known about, then don't assume a range for it */
/*		EXCEPT("double range for param '%s' not found", param);*/
		return -1;
	}
	return 0;
}

/* XXX This function probably needs a lot of work. */
static void
compute_range(const char* range, char** range_start, char** range_end) {

	const char * c1 = NULL, * c2 = NULL, * c3 = NULL;

	for (c1=range; isspace(*c1); c1++);				//skip leading whitespace

	for (c2=c1; *c2 && *c2!=','; c2++);				//find the first comma or null terminator

	// if the same or they match the generalized anything operator, there is no range
	if (c1==c2 || (strcmp(c1, ".*") == 0)) {
		*range_start = malloc(sizeof(char));
		**range_start = '\0';
		*range_end = malloc(sizeof(char));
		**range_end = '\0';
	} else {

		//find range_start
		for (c3=c2-1; isspace(*c3); c3--);			//reverse over trailing whitespace
		*range_start = calloc((c3-c1+2), sizeof(char));
		strncat(*range_start, c1, c3-c1+1);

		//find range_end
		for (c1=c2; *c1; c1++);						//find end of uppper limit, stopping at null terminator
		for (c3=c1-1; isspace(*c3); c3--);			//reverse over trailing whitespace
		for (c2++; c2<=c1 && isspace(*c2); c2++);	//skip leading whitespace
		*range_end = calloc((c3-c2+2), sizeof(char));
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
validate_regex(char* pattern, char* subject) {

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

void
iterate_params(int (*callPerElement)
							(param_info_t* /*value*/, void* /*user data*/),
				void* user_data) {
	param_info_hash_iterate(&param_info, callPerElement, user_data);
}
