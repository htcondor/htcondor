#include "condor_common.h"
#include "processid.h"

pid_t
ProcessId::getPid() const {
	return -1;
}

int
ProcessId::isConfirmed() const {
	return -1;
}

int
ProcessId::write(FILE*) const {
	return FAILURE;
}

ProcessId::ProcessId(FILE*, int&) {
}

int
ProcessId::writeConfirmationOnly(FILE*) const {
	return FAILURE;
}

ProcessId::~ProcessId() {
}
