#ifndef PARAM_FUNCTIONS_H
#define PARAM_FUNCTIONS_H

typedef char* (*PARAM_FUNC)(const char*);
typedef int (*PARAM_BOOL_INT_FUNC)(const char *, int);
typedef char* (*PARAM_WO_DEFAULT_FUNC)(const char*);
typedef int (*PARAM_INT_FUNC)(const char*, int, int, int, bool);
//typedef bool (*PARAM_BOOL_CRUFT_FUNC)(const char *, bool);

#include <stdlib.h>

class param_functions
{
public:
	param_functions() : m_param_func(NULL), m_param_bool_int_func(NULL), m_param_wo_default_func(NULL), m_param_int_func(NULL) {}
	char * param(const char *name);
	int param_boolean_int(const char *name, int default_value);
	char * param_without_default(const char *name);
	int param_integer( const char *name, int default_value = 0,
					   int min_value = INT_MIN, int max_value = INT_MAX, bool use_param_table = true );

	void set_param_func(PARAM_FUNC pf) { m_param_func = pf; }
	void set_param_bool_int_func(PARAM_BOOL_INT_FUNC pbif) { m_param_bool_int_func = pbif; }
	void set_param_wo_default_func(PARAM_WO_DEFAULT_FUNC pwodf) { m_param_wo_default_func = pwodf; }
	void set_param_int_func(PARAM_INT_FUNC pi) { m_param_int_func = pi; }

	PARAM_FUNC get_param_func() { return m_param_func; }
	PARAM_BOOL_INT_FUNC get_param_bool_int_func() { return m_param_bool_int_func; }
	PARAM_WO_DEFAULT_FUNC get_param_wo_default_func() { return m_param_wo_default_func; }
	PARAM_INT_FUNC get_param_int_func() { return m_param_int_func; }

private:
	PARAM_FUNC m_param_func;
	PARAM_BOOL_INT_FUNC m_param_bool_int_func;
	PARAM_WO_DEFAULT_FUNC m_param_wo_default_func;
	PARAM_INT_FUNC m_param_int_func;
};

#endif
