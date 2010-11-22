#include "condor_common.h"
#include "MyString.h"
#include "condor_debug.h"
#include "open_files_in_pid.h"
#include <set>

using namespace std;

int main(int argc, char *argv[])
{
	pid_t pid;
	set<MyString> files;
	set<MyString>::iterator it;

	pid = getpid();
	if (argc == 2) {
		pid = atoi(argv[1]);
	}

	files = open_files_in_pid(pid);

	for(it = files.begin(); it != files.end(); it++) {
		printf("Found open file: %s\n", (*it).Value());
	}

	return 0;
}
