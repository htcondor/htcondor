
#include "condor_common.h"
#include "condor_file_special.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "signals_control.h"

CondorFileSpecial::CondorFileSpecial(char *k)
{
	kind = k;
	readable = writeable = 1;
	_condor_signals_disable();
}

CondorFileSpecial::~CondorFileSpecial()
{
	_condor_signals_enable();
}

char * CondorFileSpecial::get_kind()
{
	return kind;
}
