
#ifndef JAVA_INFO_H
#define JAVA_INFO_H

#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "ResAttributes.h"

typedef enum {
	JAVA_INFO_STATE_VIRGIN,   /* Have not yet queried */
	JAVA_INFO_STATE_RUNNING,  /* Query in proress */
	JAVA_INFO_STATE_DONE     /* Query is done */
} java_info_state_t;

class JavaInfo : public Service {
public:
	JavaInfo();

	int compute( amask_t how_much );
	int publish( ClassAd *ad, amask_t how_much );

private:

	int query_reaper( int pid, int status );
	void query_create();

	java_info_state_t state;
	int reaper_id;
	int query_pid;
	char output_file[_POSIX_PATH_MAX];
	char error_file[_POSIX_PATH_MAX];
	char java_vendor[ATTRLIST_MAX_EXPRESSION];
	char java_version[ATTRLIST_MAX_EXPRESSION];
	float java_mflops;
	bool has_java;
};

#endif
