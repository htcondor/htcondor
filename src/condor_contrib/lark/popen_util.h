
#ifndef __popen_util_h_
#define __popen_util_h_

#include "my_popen.h"

#define RUN_ARGS_AND_LOG(prefix, name)  {\
	FILE *fp = my_popen(args, "r", TRUE); \
	if (fp == NULL) { \
		dprintf(D_ALWAYS, #prefix ": " \
			"my_popen failure on " #name ": (errno=%d) %s\n", \
			 errno, strerror(errno)); \
		return 1; \
	} \
\
	char out_buff[1024]; \
	while (fgets(out_buff, 1024, fp) != NULL) \
		dprintf(D_FULLDEBUG, #prefix ": " #name " output: %s", out_buff); \
	int status = my_pclose(fp); \
	if (status == -1) { \
		dprintf(D_ALWAYS, #prefix ": Error when running " #name " (errno=%d) %s\n", errno, strerror(errno)); \
	} else { \
		if (WIFEXITED(status)) { \
			if ((status = WEXITSTATUS(status))) { \
				dprintf(D_ALWAYS, #prefix ": " #name " exited with status %d\n", status); \
				return status; \
			} else { \
				dprintf(D_FULLDEBUG, #prefix ": " #name " was successful.\n"); \
			} \
		} else if (WIFSIGNALED(status)) { \
			status = WTERMSIG(status); \
			dprintf(D_ALWAYS, #prefix ": " #name " terminated with signal %d\n", status); \
		} \
	} \
}

#endif

