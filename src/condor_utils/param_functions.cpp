#include "condor_common.h"
#include "param_functions.h"

char * param_functions::param(const char *name)
{
	if(!m_param_func)
		return NULL;

	return m_param_func(name);
}

int param_functions::param_boolean_int(const char *name, int default_value)
{
	if(!m_param_bool_int_func)
		return default_value;

	return m_param_bool_int_func(name, default_value);
}

char * param_functions::param_without_default(const char *name)
{
	if(!m_param_wo_default_func)
		return NULL;

	return m_param_wo_default_func(name);
}

int param_functions::param_integer( const char *name, int default_value, int min_value, int max_value, bool use_param_table)
{
	if(!m_param_int_func)
		return default_value;
	return m_param_int_func(name, default_value, min_value, max_value, use_param_table);
}
