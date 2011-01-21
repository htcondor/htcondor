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
	char *dir;

	if (NULL == (dir = getenv("_CONDOR_SCRATCH_DIR"))) {
		dir = ".";
	}
	path.sprintf( "%s%c%s",dir,DIR_DELIM_CHAR,"chirp.config");
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

// Old version using open, write
// Still here because of the ability to use different modes
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

// New version using putfile
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

	FILE *rfd = ::safe_fopen_wrapper(local, "rb");


	if (rfd == NULL) {
		chirp_client_disconnect(client);
		fprintf(stderr, "Can't open local file %s\n", local);
		return -1;
	}
	
		// Get size of file, allocate buffer
	struct stat stat_buf;
	stat(local, &stat_buf);
	int size = stat_buf.st_size;
	char* buf = (char*)malloc(size);
	if ( ! buf) {
		chirp_client_disconnect(client);
		fprintf(stderr, "Can't allocate %d bytes\n", size);
		return -1;
	}
	
	int num_read = ::fread(buf, 1, size, rfd);
	
	if (num_read < 0) {
		fclose(rfd);
		free(buf);
		chirp_client_disconnect(client);
		fprintf(stderr, "local read error on %s\n", local);
		return -1;
	}
	
		// Call putfile
	int num_written = chirp_client_putfile_buffer(client, remote, buf, perm, num_read);
	if (num_written != num_read) {
		fclose(rfd);
		free(buf);
		chirp_client_disconnect(client);
		fprintf(stderr, "Couldn't chirp_write as much as we read\n");
		return -1;
	}
	
	fclose(rfd);
	free(buf);
	chirp_client_disconnect(client);
	return 0;
}


void usage() {
	printf("Usage:\n");
	printf("condor_chirp fetch  remote_file local_file\n");
	printf("condor_chirp put [-mode mode] [-perm perm] local_file "
		   "remote_file\n");
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

int chirp_put(int argc, char **argv) {
	
	int fileOffset = 2;
	char *mode = "cwt";
	unsigned perm = 0777;

	bool more = true;
	while (more && fileOffset + 1 < argc) {

		if (strcmp(argv[fileOffset], "-mode") == 0) {
			mode = argv[fileOffset+1];
			fileOffset += 2;
			more = true;
		}
		else if (strcmp(argv[fileOffset], "-perm") == 0) {
			char *permStr = argv[fileOffset + 1];
			perm = strtol(permStr, NULL, 8);
			fileOffset += 2;
			more = true;
		}
		else {
			more = false;
		}
	}

	if(fileOffset + 1 >= argc || argc > fileOffset + 2) {
		printf("condor_chirp put  [-mode mode] [-perm perm] local_file "
			   "remote_file\n");
		return -1;
	}
	
	// Use putfile
	if(strcmp(mode, "cwt") == 0 && strcmp(argv[fileOffset], "-") != 0) {
		return chirp_put_one_file(argv[fileOffset], argv[fileOffset + 1], perm);
	}
	// Use open, write
	else {
		return chirp_put_one_file(argv[fileOffset], argv[fileOffset + 1], mode, perm);
	}
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

    int rval = chirp_client_unlink(client, argv[2]);
	chirp_client_disconnect(client);
	return rval;
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
	int len = chirp_client_get_job_attr(client, argv[2], &p);
	chirp_client_disconnect(client);
	printf("%.*s\n", len, p);
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

    int rval = chirp_client_set_job_attr(client, argv[2], argv[3]);
	chirp_client_disconnect(client);
	return rval;
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

    int rval = chirp_client_ulog(client, argv[2]);
	chirp_client_disconnect(client);
	return rval;
}

int chirp_read(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp read remotepath length\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}


	int fd = chirp_client_open(client, argv[2], "r", 0);
	int size = atoi(argv[3]);
	void* buf = malloc(size+1);
	int ret_val = chirp_client_read(client, fd, buf, size);
	int temp_errno = errno;
	if(ret_val >= 0) {
		char* to_print = (char*)buf;
		to_print[size] = '\0';
		printf("%s\n", to_print);
	}
	
	chirp_client_close(client, fd);
	chirp_client_disconnect(client);
	errno = temp_errno;
	return ret_val;
}

int chirp_rmdir(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp rmdir remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int status = chirp_client_rmdir(client, argv[2]);
	chirp_client_disconnect(client);
	return status;
}

int chirp_pread(int argc, char **argv) {
	if (argc != 5) {
		printf("condor_chirp pread path length offset\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}
		
		// Open the remote file
	int fd = chirp_client_open(client, argv[2], "r", 0);
	if(fd < 0) {
		chirp_client_disconnect(client);
		return fd;
	}

	int size = atoi(argv[3]);
	int offset = atoi(argv[4]);
	void* buf = malloc(size + 1);
	int ret_val = chirp_client_pread(client, fd, buf, size, offset);
	int temp_errno = errno;
	if(ret_val >= 0) {
		char* to_print = (char*)buf;
		to_print[size] = '\0';
		printf("%s\n", to_print);
	}

	chirp_client_close(client, fd);
	chirp_client_disconnect(client);
	errno = temp_errno;
	return ret_val;
}

int chirp_pwrite(int argc, char **argv) {
	if (argc != 5) {
		printf("condor_chirp pwrite remotepath offset localfile\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int offset = atoi(argv[3]);
	int num_read = 0, num_written = 0, size = 0;
	FILE *rfd;
	char *buf;
	buf = (char*)malloc(1024);
	
		// Use stdin or a local file
	if (strcmp(argv[4], "-") == 0) {
		rfd = stdin;
	} else {
		rfd = ::safe_fopen_wrapper(argv[4], "rb");
		if (!rfd) {
			free((char*)buf);
			chirp_client_disconnect(client);
			fprintf(stderr, "unable to open file %s\n", argv[4]);
			return -1;
		}
	}
	
		// Open the remote file
	int fd = chirp_client_open(client, argv[2], "w", 0);
	if(fd < 0) {
		chirp_client_disconnect(client);
		return fd;
	}

		// Do the pwrite(s)
	do {
		num_read = ::fread(buf, 1, 1024, rfd);
		if (num_read < 0) {
			fclose(rfd);
			free((char*)buf);
			chirp_client_close(client, fd);
			chirp_client_disconnect(client);
			fprintf(stderr, "local read error on %s\n", argv[4]);
			return -1;
		}

			// EOF
		if (num_read == 0) {
			break;
		}
		
		num_written = chirp_client_pwrite(client, fd, buf, num_read, offset+size);
			
			// Make sure we wrote the expected number of bytes
		if(num_written != num_read) {
			fclose(rfd);
			free((char*)buf);
			int temp_errno = errno;
			chirp_client_close(client, fd);
			chirp_client_disconnect(client);
			errno = temp_errno;
			fprintf(stderr, "pwrite unable to write %d bytes\n", num_read);
			return -1;
		}

		size += num_read;
	} while (num_read > 0);
	
	free((char*)buf);
	chirp_client_close(client, fd);
	chirp_client_disconnect(client);
	return 0;
}

int chirp_swrite(int argc, char **argv) {
	if (argc != 7) {
		printf("condor_chirp swrite remotepath offset stridelength strideskip "
			"localfile\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}
	int offset = atoi(argv[3]);
	int stride_length = atoi(argv[4]);
	int stride_skip = atoi(argv[5]);
	
	struct stat stat_buf;
	stat(argv[6], &stat_buf);
	int size = stat_buf.st_size;
	char* buf = (char*)malloc(size);
	FILE *rfd = ::safe_fopen_wrapper(argv[6], "rb");
	if (!rfd) {
		chirp_client_disconnect(client);
		fprintf(stderr, "unable to open local file %s\n", argv[6]);
		free((char*)buf);
		return -1;
	}

	int num_read = ::fread(buf, 1, size, rfd);
	if (num_read < 0) {
		fclose(rfd);
		chirp_client_disconnect(client);
		fprintf(stderr, "local read error on %s\n", argv[6]);
		free((char*)buf);
		return -1;
	}
	
	int fd = chirp_client_open(client, argv[2], "w", 0);
	if(fd < 0) {
		chirp_client_disconnect(client);
		free((char*)buf);
		return fd;
	}
	int num_written = chirp_client_swrite(client, fd, buf, size, offset,
										  stride_length, stride_skip);
	if(num_written < 0) {
		fclose(rfd);
		free(buf);
		int temp_errno = errno;
		chirp_client_close(client, fd);
		chirp_client_disconnect(client);
		errno = temp_errno;
		return num_written;
	}

	if(num_written != num_read) {
		fprintf(stderr, "Couldn't swrite(%d) as much as we read(%d)\n",
			num_written, num_read);
	}
	
	fclose(rfd);
	free(buf);
	chirp_client_close(client, fd);
	chirp_client_disconnect(client);
	return num_written;
}

int chirp_rmall(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp rmall remotepath\n");
		return -1;
	}

	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int status = chirp_client_rmall(client, argv[2]);
	chirp_client_disconnect(client);
	return status;
}

int chirp_getlongdir(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp getlongdir remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	char *buffer;
	int status = chirp_client_getlongdir(client, argv[2], &buffer);
	char *line;
	struct chirp_stat stat;
	
	line = strtok(buffer, "\n");
	while(line != NULL) {
		printf("%s\n", line);
		line = strtok(NULL, "\n");
		if(line == NULL || get_stat(line, &stat) < 0) {
			break;
		}
		print_stat(&stat, "\t");
		line = strtok(NULL, "\n");
	}
	chirp_client_disconnect(client);
	return status;
}

int chirp_getdir(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp getdir remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}
	char *buffer;
	int status = chirp_client_getdir(client, argv[2], &buffer);
	chirp_client_disconnect(client);
	printf("%s\n", buffer);
	return status;
}

int chirp_whoami() {
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}
	
	char* buffer = (char*)malloc(CHIRP_LINE_MAX);
	int status = chirp_client_whoami(client, buffer, CHIRP_LINE_MAX);
	chirp_client_disconnect(client);
	printf("%s\n", buffer);
	free(buffer);
	return status;
}

int chirp_whoareyou(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp whoareyou remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}
	
	char* buffer = (char*)malloc(CHIRP_LINE_MAX);
	int status = chirp_client_whoareyou(client, argv[2], buffer, CHIRP_LINE_MAX);
	chirp_client_disconnect(client);
	printf("%s\n", buffer);
	free(buffer);
	return status;
}

int chirp_link(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp link oldpath newpath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int status = chirp_client_link(client, argv[2], argv[3]);
	chirp_client_disconnect(client);
	return status;
}

int chirp_symlink(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp symlink oldpath newpath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int status = chirp_client_symlink(client, argv[2], argv[3]);
	chirp_client_disconnect(client);
	return status;
}

int chirp_readlink(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp readlink remotepath length\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	char *buffer = NULL;

	int length = atoi(argv[3]);
	int status = chirp_client_readlink(client, argv[2], length, &buffer);
	chirp_client_disconnect(client);
	if(status >= 0) {
		printf("%s\n", buffer);
	}
	return status;
}

int chirp_sstat(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp stat remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}
	struct chirp_stat stat;
	int status = chirp_client_stat(client, argv[2], &stat);
	chirp_client_disconnect(client);
	if(status >= 0) {
		print_stat(&stat);
	}
	return status;
}

int chirp_lstat(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp lstat remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}
	
	struct chirp_stat stat;
	int status = chirp_client_lstat(client, argv[2], &stat);
	chirp_client_disconnect(client);
	if(status >= 0) {
		print_stat(&stat);
	}
	return status;
}

int chirp_sstatfs(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp statfs remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	struct chirp_statfs statfs;
	int status = chirp_client_statfs(client, argv[2], &statfs);
	chirp_client_disconnect(client);
	if(status >= 0) {
		print_statfs(&statfs);
	}
	return status;
}

int chirp_access(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp access remotepath mode(rwxf)\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int mode = 0; 
	char *m = argv[3];
	while(*m != '\0') {
		switch(*m) {
			case 'r':
				mode |= R_OK;
				break;
			case 'w':
				mode |= W_OK;
				break;
			case 'x':
				mode |= X_OK;
				break;
			case 'f':
				mode |= F_OK;
				break;
			default:
				fprintf(stderr, "invalid mode char '%c'\n", *m);
				return -1;
				break;
		}
		m++;
	}
	int status = chirp_client_access(client, argv[2], mode);
	chirp_client_disconnect(client);
	return status;
}

int chirp_chmod(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp chmod remotepath mode\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	unsigned mode;
	sscanf(argv[3], "%o", &mode);
	int status = chirp_client_chmod(client, argv[2], mode);
	chirp_client_disconnect(client);
	return status;
}

int chirp_chown(int argc, char **argv) {
	if (argc != 5) {
		printf("condor_chirp chown remotepath uid gid\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

		// First, connect to the submit host
	client = chirp_client_connect_starter();
	if (!client) {
		fprintf(stderr, "cannot chirp_connect to shadow\n");
		return -1;
	}

	int uid = atoi(argv[3]);
	int gid = atoi(argv[4]);
	int status = chirp_client_chown(client, argv[2], uid, gid);
	chirp_client_disconnect(client);
	return status;
}

int chirp_lchown(int argc, char **argv) {
	if (argc != 5) {
		printf("condor_chirp lchown remotepath uid gid\n");
		return -1;
	}
	
	struct chirp_client *client = 0;

	if (argc == 1) {
		usage();
		exit(-1);
	}

	int uid = atoi(argv[3]);
	int gid = atoi(argv[4]);
	int status = chirp_client_lchown(client, argv[2], uid, gid);
	chirp_client_disconnect(client);
	return status;
}

int chirp_truncate(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp lchown remotepath length\n");
		return -1;
	}

	if (strcmp("put", argv[1]) == 0) {
		return chirp_put(argc, argv);
	}

	int len = atoi(argv[3]);
	int status = chirp_client_truncate(client, argv[2], len);
	chirp_client_disconnect(client);
	return status;
}

int chirp_utime(int argc, char **argv) {
	if (argc != 5) {
		printf("condor_chirp utime remotepath actime mtime\n");
		return -1;
	}

	if (strcmp("get_job_attr", argv[1]) == 0) {
		return chirp_get_job_attr(argc, argv);
	}
	
	int actime = atoi(argv[3]);
	int mtime = atoi(argv[4]);
	int status = chirp_client_utime(client, argv[2], actime, mtime);
	chirp_client_disconnect(client);
	return status;
}

void usage() {
	printf("Usage:\n");
	printf("condor_chirp fetch  remote_file local_file\n");
	printf("condor_chirp put [-mode mode] [-perm perm] local_file "
		   "remote_file\n");
	printf("condor_chirp remove remote_file\n");
	printf("condor_chirp get_job_attr job_attribute\n");
	printf("condor_chirp set_job_attr job_attribute attribute_value\n");
	printf("condor_chirp ulog text\n");
	printf("condor_chirp pread path length offset\n");
	printf("condor_chirp pwrite remotepath offset localfile\n");
	printf("condor_chirp swrite remotepath offset stridelength strideskip "
		"localfile\n");
	printf("condor_chirp rmall remotepath\n");
	printf("condor_chirp getlongdir remotepath\n");
	printf("condor_chirp getdir remotepath\n");
	printf("condor_chirp whoami\n");
	printf("condor_chirp whoareyou remotepath\n");
	printf("condor_chirp link oldpath newpath\n");
	printf("condor_chirp symlink oldpath newpath\n");
	printf("condor_chirp readlink remotepath length\n");
	printf("condor_chirp stat remotepath\n");
	printf("condor_chirp lstat remotepath\n");
	printf("condor_chirp statfs remotepath\n");
	printf("condor_chirp access remotepath mode(rwxf)\n");
	printf("condor_chirp chmod remotepath mode\n");
	printf("condor_chirp chown remotepath uid gid\n");
	printf("condor_chirp lchown remotepath uid gid\n");
	printf("condor_chirp truncate remotepath length\n");
	printf("condor_chirp utime remotepath actime mtime\n");
}

int
main(int argc, char **argv) {

	int ret_val = -1;

	if (strcmp("set_job_attr", argv[1]) == 0) {
		return chirp_set_job_attr(argc, argv);
	}

	if (strcmp("ulog", argv[1]) == 0) {
		return chirp_ulog(argc, argv);
	}

	if(ret_val < 0 && errno != 0) {
		printf("\tError: %d (%s)\n", errno, strerror(errno));
	}
	return ret_val;
	exit(-1);
}
