
#include "condor_common.h"
#include "condor_file_special.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "signals_control.h"

CondorFileSpecial::CondorFileSpecial() : CondorFileLocal()
{
	readable = writeable = 1;
	_condor_ckpt_disable();
}

CondorFileSpecial::~CondorFileSpecial()
{
	_condor_ckpt_enable();
}

int CondorFileSpecial::attach( int fd_in )
{
	fd = fd_in;
	readable = writeable = 1;
	size = -1;
	return 0;
}

