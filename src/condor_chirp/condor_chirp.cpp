/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

/*
 * condor_chirp -- a program wrapper around the chirp API
 */


#include "condor_common.h"
#include "chirp_client.h"
#include "chirp_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include "MyString.h"

struct chirp_client *
chirp_client_connect_starter()
{
    FILE *file;
    int fields;
    int save_errno;
    struct chirp_client *client;
    char host[CONDOR_HOSTNAME_MAX];
    char cookie[CHIRP_LINE_MAX];
	MyString path;
    int port;
    int result;

	path.sprintf( "%s%c%s",getenv("_CONDOR_SCRATCH_DIR"),DIR_DELIM_CHAR,"chirp.config");
    file = safe_fopen_wrapper(path.Value(),"r");
    if(!file) { 
		fprintf(stderr, "Can't open %s file\n",path.Value());
		return 0;
	}

    fields = fscanf(file,"%s %d %s",host,&port,cookie);
    fclose(file);

    if(fields!=3) {
        errno = EINVAL;
        return 0;
    }

    client = chirp_client_connect(host,port);
    if(!client) return 0;

    result = chirp_client_cookie(client,cookie);
    if(result!=0) {
        save_errno = errno;
        chirp_client_disconnect(client);
        errno = save_errno;
        return 0;
    }

    return client;
}

int
chirp_get_one_file(char *remote, char *local) {
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int rfd = chirp_client_open(client, remote, "r", 0);
	if (rfd < 0) {
		fprintf(stderr, "Can't chirp_open %s\n", remote);
		chirp_client_disconnect(client);
		return -1;
	}

	FILE *wfd;
	if (strcmp(local, "-") == 0) {
		wfd = stdout;
	} else {
		wfd = ::safe_fopen_wrapper(local, "wb+");
	}

	if (wfd == NULL) {
		fprintf(stderr, "can't open local filename %s: %s\n", local, strerror(errno));
		chirp_client_close(client, rfd);
		chirp_client_disconnect(client);
		return -1;
	}

	char buf[8192];

	int num_read = 0;
	do {
		num_read = chirp_client_read(client, rfd, buf, 8192);
		if (num_read < 0) {
			fprintf(stderr, "couldn't chirp_read\n");
			::fclose(wfd);
			chirp_client_close(client, rfd);
			chirp_client_disconnect(client);
			return -1;
		}

		int num_written = ::fwrite(buf, 1, num_read, wfd);
		if (num_written < 0) {
			fprintf(stderr, "local read error on %s\n", local);
			::fclose(wfd);
			chirp_client_close(client, rfd);
			chirp_client_disconnect(client);
			return -1;
		}

	} while (num_read > 0);

	::fclose(wfd);
	chirp_client_close(client, rfd);
	chirp_client_disconnect(client);
	return 0;
}

int
chirp_put_one_file(char *local, char *remote, char *mode, int perm) {
	struct chirp_client *client;

		// We connect each time, so that we don't have thousands
		// of idle connections hanging around the master
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "Can't connect to chirp server\n");
		exit(-1);
	}

	FILE *rfd;
	if (strcmp(local, "-") == 0) {
		rfd = stdin;
	} else {
		rfd = ::safe_fopen_wrapper(local, "rb");
	}

	if (rfd == NULL) {
		chirp_client_disconnect(client);
		fprintf(stderr, "Can't open local file %s\n", local);
		return -1;
	}

	int wfd = chirp_client_open(client, remote, mode, perm);
	if (wfd < 0) {
		::fclose(rfd);
		chirp_client_disconnect(client);
		fprintf(stderr, "Can't chirp_client_open %s:%d\n", remote, wfd);
		return -1;
	}

	char buf[8192];

	int num_read = 0;
	do {
		num_read = ::fread(buf, 1, 8192, rfd);
		if (num_read < 0) {
			fclose(rfd);
			chirp_client_close(client, wfd);
			chirp_client_disconnect(client);
			fprintf(stderr, "local read error on %s\n", local);
			return -1;
		}

			// EOF
		if (num_read == 0) {
			break;
		}

		int num_written = chirp_client_write(client, wfd, buf, num_read);
		if (num_written != num_read) {
			fclose(rfd);
			chirp_client_close(client, wfd);
			chirp_client_disconnect(client);
			fprintf(stderr, "Couldn't chirp_write as much as we read\n");
			return -1;
		}

	} while (num_read > 0);
	::fclose(rfd);
	chirp_client_close(client, wfd);
	chirp_client_disconnect(client);
		
	return 0;
}


void usage() {
	printf("Usage:\n");
	printf("condor_chirp fetch  remote_file local_file\n");
	printf("condor_chirp put    local_file remote_file\n");
	printf("condor_chirp remove remote_file\n");
	printf("condor_chirp get_job_attr job_attribute\n");
	printf("condor_chirp set_job_attr job_attribute attribute_value\n");
	printf("condor_chirp ulog text\n");
}

/*
 * chirp_fetch
 *   parse the fetch-specific arguments, then call
 *  chirp_get_one_file to do the real work
 */

int chirp_fetch(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp fetch remote_file local_file\n");
		return -1;
	}
	return chirp_get_one_file(argv[2], argv[3]);
}

/*
 * chirp_put
 *   parse the put-specific arguments, then call
 *  chirp_put_one_file to do the real work
 */

int chirp_put(int /* argc */, char **argv) {
	
	int fileOffset = 2;
	char *mode = "cwat";
	int  perm = 0777;

	bool more = true;
	while (more) {

		more = false;
		if (strcmp(argv[fileOffset], "-mode") == 0) {
			mode = argv[fileOffset + 1];
			fileOffset += 2;
			more = true;
		}

		if (strcmp(argv[fileOffset], "-perm") == 0) {
			char *permStr = argv[fileOffset + 1];
			perm = strtol(permStr, NULL, 0);
			fileOffset += 2;
			more = true;
		}
	}

	return chirp_put_one_file(argv[fileOffset], argv[fileOffset + 1], mode, perm);
}

/*
 * chirp_remove
 *   call chirp_remove to do the real work
 */

int chirp_remove(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp remove remote_file\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

    return chirp_client_unlink(client, argv[2]);
}
	
/*
 * chirp_getattr
 *   call chirp_getattr to do the real work
 */

int chirp_get_job_attr(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp get_job_attr AttributeName\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	char *p = 0;
    	chirp_client_get_job_attr(client, argv[2], &p);
	printf("%s\n", p);
	return 0;
}

/*
 * chirp_getattr
 *   call chirp_setattr to do the real work
 */

int chirp_set_job_attr(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp set_job_attr AttributeName AttributeValue\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

    return chirp_client_set_job_attr(client, argv[2], argv[3]);
}

/*
 * chirp_ulog
 *   
 */

int chirp_ulog(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp ulog message\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

    return chirp_client_ulog(client, argv[2]);
}

int
main(int argc, char **argv) {

	if (argc == 1) {
		usage();
		exit(-1);
	}

	if (strcmp("fetch", argv[1]) == 0) {
		return chirp_fetch(argc, argv);
	}

	if (strcmp("put", argv[1]) == 0) {
		return chirp_put(argc, argv);
	}

	if (strcmp("remove", argv[1]) == 0) {
		return chirp_remove(argc, argv);
	}

	if (strcmp("get_job_attr", argv[1]) == 0) {
		return chirp_get_job_attr(argc, argv);
	}

	if (strcmp("set_job_attr", argv[1]) == 0) {
		return chirp_set_job_attr(argc, argv);
	}

	if (strcmp("ulog", argv[1]) == 0) {
		return chirp_ulog(argc, argv);
	}

	usage();
	exit(-1);
}
