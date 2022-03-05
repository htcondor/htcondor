#include "procapi.h"

int
ProcAPI::getProcInfo( pid_t, piPTR&, int& ) {
	return PROCAPI_FAILURE;
}

int
ProcAPI::getPSSInfo(int, procInfo&, int& ) {
	return PROCAPI_FAILURE;
}

size_t
pidHashFunc(const pid_t&) {
	return PROCAPI_FAILURE;
}

procInfo *
ProcAPI::getProcInfoList() {
	return NULL;
}


int
ProcAPI::getProcSetInfo(pid_t*, int, piPTR&, int&) {
	return PROCAPI_FAILURE;
}

int
ProcAPI::confirmProcessId(ProcessId&, int&) {
	return PROCAPI_FAILURE;
}

int
ProcAPI::getPidFamilyByLogin(const char*, ExtArray<pid_t>&) {
	return PROCAPI_FAILURE;
}

int
ProcAPI::createProcessId(pid_t, ProcessId*&, int&, int*) {
	return PROCAPI_FAILURE;
}

int
ProcAPI::getPidFamily(pid_t, PidEnvID_s*, ExtArray<int>&, int&) {
	return PROCAPI_FAILURE;
}

int
ProcAPI::isAlive(const ProcessId&, int&) {
	return PROCAPI_FAILURE;
}

size_t
ProcAPI::getBasicUsage(pid_t, double *, double *) {
	return PROCAPI_FAILURE;
}
