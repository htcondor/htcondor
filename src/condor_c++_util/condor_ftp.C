#include "condor_common.h"
#include "MyString.h"
#include "condor_ftp.h"

void stm_to_string(SandboxTransferMethod stm, MyString &str)
{
	switch(stm)
	{
		case STM_USE_SCHEDD_ONLY:
			str = "STM_USE_SCHEDD_ONLY";
			break;

		case STM_USE_TRANSFERD:
			str = "STM_USE_TRANSFERD";
			break;

		default:
			str = "STM_UNKNOWN";
			break;
	}

	str = "STM_UNKNOWN";
}

void string_to_stm(const MyString &str, SandboxTransferMethod &stm)
{
	MyString tmp;

	tmp = str;

	tmp.trim();
	tmp.upper_case();

	// a default value incase nothing matches.
	stm = STM_UNKNOWN;

	if (tmp == "STM_USE_SCHEDD_ONLY") {
		stm = STM_USE_SCHEDD_ONLY;

	} else if (tmp == "STM_USE_TRANSFERD") {
		stm = STM_USE_TRANSFERD;
	}
}
