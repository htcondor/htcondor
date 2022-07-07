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
#  define PCRE2_CODE_UNIT_WIDTH 8
#  include <pcre2.h>
#endif

#define PARAM_DECLARE_TABLES 1 // so param_info_table will give us the table declarations.
#include "param_info_tables.h"
int param_info_init(const void ** pvdefaults)
{
	*pvdefaults = condor_params::defaults;
	return condor_params::defaults_count;
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

// case insensitive compare of two strings up to the first ":" or \0
//
int ComparePrefixBeforeColon(const char * p1, const char * p2)
{
	for (;*p1 || *p2; ++p1, ++p2) {
		int ch1 = *p1, ch2 = *p2;
		if (ch1 == ':') ch1 = 0; else if (ch1 >= 'a') ch1 &= ~0x20; // change lower to upper
		if (ch2 == ':') ch2 = 0; else if (ch2 >= 'a') ch2 &= ~0x20; // this is cheap, but doesn't work for {|}~
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

// returns the subsystem table for the given defaults table and subsystem.
int param_get_subsys_table(const void* pvdefaults, const char* subsys, MACRO_DEF_ITEM** ppTable)
{
	*ppTable = NULL;
	if ( ! pvdefaults || (pvdefaults == condor_params::defaults)) {
		MACRO_TABLE_PAIR* subtab = NULL;
		subtab = BinaryLookup<MACRO_TABLE_PAIR>(
			condor_params::subsystems,
			condor_params::subsystems_count,
			subsys, ComparePrefixBeforeDot);
		if (subtab) {
			*ppTable = subtab->aTable;
			return subtab->cElms;
		}
	}
	return 0;
}

MACRO_DEF_ITEM * param_default_lookup2(const char * param, const char * subsys)
{
	if (subsys) {
		MACRO_DEF_ITEM * p = param_subsys_default_lookup(subsys, param);
		if (p) return p;
		// fall through to do generic lookup.
	}
	return param_default_lookup(param);
}

const char * param_meta_value (
	const struct condor_params::ktp_value & knobsets,
	const char * meta,
	const char * param,
	int * pmeta_id)
{
	MACRO_TABLE_PAIR * pt = BinaryLookup<MACRO_TABLE_PAIR>(
		knobsets.aTables,
		knobsets.cTables,
		meta, ComparePrefixBeforeColon);
	if (pt) {
		MACRO_DEF_ITEM * p = param_meta_table_lookup(pt, param, pmeta_id);
		if (p && p->def) {
			if (pmeta_id) {
				int tbl_index = (int)(pt - knobsets.aTables);
				while (tbl_index > 0) {
					*pmeta_id += knobsets.aTables[--tbl_index].cElms;
				}
			}
			return p->def->psz;
		}
	}
	if (pmeta_id) { *pmeta_id = -1; }
	return nullptr;
}

const char * param_meta_value(const char * meta, const char * param, int * pmeta_id)
{
	return param_meta_value(condor_params::def_metaknobsets, meta, param, pmeta_id);
}

MACRO_DEF_ITEM * param_meta_source_by_id(int meta_id, MACRO_TABLE_PAIR ** ptable)
{
	// the meta_id is the sum of the index within the table
	// and sum of entries in all of the tables before
	if (meta_id < 0)
		return NULL;
	for (int ix = 0; ix < condor_params::metaknobsets_count; ++ix) {
		if (meta_id < condor_params::metaknobsets[ix].cElms) {
			if (ptable) *ptable = &condor_params::metaknobsets[ix];
			return condor_params::metaknobsets[ix].aTable + meta_id;
		}
		meta_id -= condor_params::metaknobsets[ix].cElms;
	}
	return NULL;
}

// lookup a knob by metaname and by knobname.
//
MACRO_TABLE_PAIR* param_meta_table(const struct condor_params::ktp_value & knobsets, const char * meta, int * base_meta_id)
{
	MACRO_TABLE_PAIR * pt = BinaryLookup<MACRO_TABLE_PAIR>(
		knobsets.aTables,
		knobsets.cTables,
		meta, ComparePrefixBeforeColon);
	if (pt) {
		if (base_meta_id) {
			int meta_id = 0;
			int tbl_index = (int)(pt - knobsets.aTables);
			while (tbl_index > 0) {
				meta_id += condor_params::metaknobsets[--tbl_index].cElms;
			}
			*base_meta_id = meta_id;
		}
		return pt;
	}
	if (base_meta_id) { *base_meta_id = 0; }
	return nullptr;
}

MACRO_TABLE_PAIR* param_meta_table(const char * meta, int * base_meta_id) {
	return param_meta_table(condor_params::def_metaknobsets, meta, base_meta_id);
}

// lookup a param in the metatable
MACRO_DEF_ITEM * param_meta_table_lookup(MACRO_TABLE_PAIR* table, const char * param, int * meta_offset)
{
	if (table) {
		MACRO_DEF_ITEM * p = BinaryLookup<MACRO_DEF_ITEM>(table->aTable, table->cElms, param, strcasecmp);
		if (p && meta_offset) {
			*meta_offset = (int)(p - table->aTable);
		}
		return p;
	}
	if (meta_offset) {
		*meta_offset = -1;
	}
	return NULL;
}

const char * param_meta_table_string(MACRO_TABLE_PAIR * table, const char * param, int * meta_offset)
{
	if (table) {
		MACRO_DEF_ITEM * p = BinaryLookup<MACRO_DEF_ITEM>(table->aTable, table->cElms, param, strcasecmp);
		if (p && p->def) {
			if (meta_offset) { *meta_offset = (int)(p - table->aTable); }
			return p->def->psz;
		}
	}
	if (meta_offset) {
		*meta_offset = -1;
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

int param_default_get_id(const char*param, const char * * ppdot)
{
	if (ppdot) *ppdot = NULL;
	int ix = -1;
	const param_table_entry_t* found = param_generic_default_lookup(param);
	if ( ! found) {
		const char * pdot = strchr(param, '.');
		if (pdot) {
			if (ppdot) *ppdot = pdot+1;
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

param_info_t_type_t param_default_range_by_id(int ix, const int *&imin, const double*&dmin, const long long*&lmin)
{
	imin = NULL; dmin = NULL; lmin = NULL;
	if (ix >= 0 && ix < condor_params::defaults_count) {
		const param_table_entry_t* p = &condor_params::defaults[ix];
		if (p && p->def) {
			int flags = reinterpret_cast<const condor_params::string_value *>(p->def)->flags;
			if (flags & condor_params::PARAM_FLAGS_RANGED) {
				switch (flags & condor_params::PARAM_FLAGS_TYPE_MASK) {
				case PARAM_TYPE_INT:
					imin = &(reinterpret_cast<const condor_params::ranged_int_value *>(p->def)->min);
					return PARAM_TYPE_INT;
				case PARAM_TYPE_LONG:
					lmin = &(reinterpret_cast<const condor_params::ranged_long_value *>(p->def)->min);
					return PARAM_TYPE_LONG;
				case PARAM_TYPE_DOUBLE:
					dmin = &(reinterpret_cast<const condor_params::ranged_double_value *>(p->def)->min);
					return PARAM_TYPE_DOUBLE;
				}
			}
		}
	}
	return PARAM_TYPE_STRING;
}


const char*
param_default_string(const char* param, const char * subsys)
{
	const param_table_entry_t* p = param_default_lookup2(param, subsys);
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
param_default_integer(const char* param, const char* subsys, int* valid, int* is_long, int* truncated) {
	int ret = 0;
	if (valid) *valid = false;
	if (is_long) *is_long = false;
	if (truncated) *truncated = false;
	const param_table_entry_t* p = param_default_lookup2(param, subsys);
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
					if (truncated) *truncated = true;
				};
				if (valid) *valid = true;
				if (is_long) *is_long = true;
				}
				break;
		}
	}
	return ret;
}

int
param_default_boolean(const char* param, const char* subsys, int* valid) {
	return (param_default_long(param, subsys, valid) != 0);
}

long long
param_default_long(const char* param, const char* subsys, int* valid) {
	int ret = 0;
	if (valid) *valid = false;
	const param_table_entry_t* p = param_default_lookup2(param, subsys);
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
	return ret;
}

double
param_default_double(const char* param, const char * subsys, int* valid) {
	double ret = 0.0;

	const param_table_entry_t* p = param_default_lookup2(param, subsys);
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
	return ret;
}

int
param_range_long(const char* param, long long* min, long long* max) {

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
}

int
param_range_integer(const char* param, int* min, int* max) {

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
}

int
param_range_double(const char* param, double* min, double* max) {

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
}

