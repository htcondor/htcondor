/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_distribution.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_privsep.h"

int
main(int argc, char* argv[])
{
	if (argc != 4) {
		fprintf(stderr,
		        "usage: test_privsep_open <uid> <gid> <file_name>\n");
		return EXIT_FAILURE;
	}

	myDistro->Init(argc, argv);

	config();

	Termlog = 1;
	dprintf_config("TOOL");

	uid_t uid = atoi(argv[1]);
	gid_t gid = atoi(argv[2]);

	int fd = privsep_open(uid, gid, argv[3], O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "error: privsep_open failed\n");
		return EXIT_FAILURE;
	}
	FILE* fp = fdopen(fd, "r");
	if (fp == NULL) {
		fprintf(stderr, "fdopen error: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	while (1) {
		char buffer[1024];
		size_t bytes = fread(buffer, 1, 1024, fp);
		if (bytes == 0) {
			break;
		}
		bytes = fwrite(buffer, 1, bytes, stdout);
		if (bytes == 0) {
			fprintf(stderr, "fwrite error: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
	}
	if (ferror(fp)) {
		fprintf(stderr, "fread error: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	fclose(fp);

	return EXIT_SUCCESS;
}
