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

// the documentation for the chirp tool is that it returns 1 on failure, so that is what we will do.
// when the chirp_client API calls abort().
void abort_handler(int /*signum*/) { exit(1); }

#define DISCONNECT_AND_RETURN(client, rval) \
	int save_errno = errno; \
	chirp_client_disconnect((client)); \
	errno = save_errno; \
	return (rval)

#define CLOSE_DISCONNECT_AND_RETURN(client, fd, rval) \
	int save_errno = errno; \
	chirp_client_close((client), (fd)); \
	chirp_client_disconnect((client)); \
	errno = save_errno; \
	return (rval)

#define CONNECT_STARTER(client) \
	(client) = chirp_client_connect_starter(); \
	if (!(client)) { \
		fprintf(stderr, "cannot chirp_connect to condor_starter\n"); \
		return -1; \
	}

void print_stat(chirp_stat *stat, const char *lead = "") {
	time_t t;
	
	printf("%sdevice: %ld\n", lead, stat->cst_dev);
	printf("%sinode: %ld\n", lead, stat->cst_ino);
	printf("%smode: %ld\n", lead, stat->cst_mode);
	printf("%snlink: %ld\n", lead, stat->cst_nlink);
	printf("%suid: %ld\n", lead, stat->cst_uid);
	printf("%sgid: %ld\n", lead, stat->cst_gid);
	printf("%srdevice: %ld\n", lead, stat->cst_rdev);
	printf("%ssize: %ld\n", lead, stat->cst_size);
	printf("%sblksize: %ld\n", lead, stat->cst_blksize);
	printf("%sblocks: %ld\n", lead, stat->cst_blocks);
	t = stat->cst_atime;
	printf("%satime: %s", lead, ctime(&t));
	t = stat->cst_mtime;
	printf("%smtime: %s", lead, ctime(&t));
	t = stat->cst_ctime;
	printf("%sctime: %s", lead, ctime(&t));
}

void print_statfs(chirp_statfs *stat, const char *lead = "") {
	printf("%sf_type: %ld\n", lead, stat->f_type);
	printf("%sf_bsize: %ld\n", lead, stat->f_bsize);
	printf("%sf_blocks: %ld\n", lead, stat->f_blocks);
	printf("%sf_bfree: %ld\n", lead, stat->f_bfree);
	printf("%sf_bavail: %ld\n", lead, stat->f_bavail);
	printf("%sf_files: %ld\n", lead, stat->f_files);
	printf("%sf_ffree: %ld\n", lead, stat->f_ffree);
}

struct chirp_client *
chirp_client_connect_starter()
{
    FILE *file;
    int fields;
    struct chirp_client *client;
    char *default_filename;
    char host[CONDOR_HOSTNAME_MAX];
    char cookie[CHIRP_LINE_MAX];
	std::string path;
    int port;
    int result;
	const char *dir;

	if ((default_filename = getenv("_CONDOR_CHIRP_CONFIG"))) {
		path = default_filename;
	} else {
		if (NULL == (dir = getenv("_CONDOR_SCRATCH_DIR"))) {
			dir = ".";
		}
		path = dir;
		path += DIR_DELIM_CHAR;
		path += ".chirp.config";
	}
    file = safe_fopen_wrapper_follow(path.c_str(),"r");
    if(!file) {
		fprintf(stderr, "Can't open %s file\n",path.c_str());
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
		DISCONNECT_AND_RETURN(client, 0);
    }

    return client;
}

int
chirp_get_one_file(char *remote, char *local) {
	struct chirp_client *client = 0;

	CONNECT_STARTER(client);
	
	char *buf = NULL;
	
	int num_read = chirp_client_getfile_buffer( client, remote, &buf );
	if (num_read < 0) {
		DISCONNECT_AND_RETURN(client, -1);
	}
	
	FILE *wfd;
	if (strcmp(local, "-") == 0) {
		wfd = stdout;
	} else {
		wfd = ::safe_fopen_wrapper_follow(local, "wb+");
	}

	if (wfd == NULL) {
		fprintf(stderr, "Can't open local file %s\n", local);
		free(buf);
		DISCONNECT_AND_RETURN(client, -1);
	}
	
	
	int num_written = ::fwrite(buf, 1, num_read, wfd);
	if (num_written != num_read) {
		fprintf(stderr, "local write error on %s\n", local);
		::fclose(wfd);
		free(buf);
		DISCONNECT_AND_RETURN(client, -1);
	}
	
	fclose(wfd);
	free(buf);
	DISCONNECT_AND_RETURN(client, 0);
}

// Old version using open, write
// Still here because of the ability to use different modes
int
chirp_put_one_file(char *local, char *remote, char *mode, int perm) {
	struct chirp_client *client;

		// We connect each time, so that we don't have thousands
		// of idle connections hanging around the master
	CONNECT_STARTER(client);

	FILE *rfd;
	if (strcmp(local, "-") == 0) {
		rfd = stdin;
	} else {
		rfd = ::safe_fopen_wrapper_follow(local, "rb");
	}

	if (rfd == NULL) {
		fprintf(stderr, "Can't open local file %s\n", local);
		DISCONNECT_AND_RETURN(client, -1);
	}

	int wfd = chirp_client_open(client, remote, mode, perm);
	if (wfd < 0) {
		::fclose(rfd);
		fprintf(stderr, "Can't chirp_client_open %s:%d\n", remote, wfd);
		DISCONNECT_AND_RETURN(client, -1);
	}

	char buf[8192];

	int num_read = 0;
	do {
		num_read = ::fread(buf, 1, 8192, rfd);
		if (num_read < 0) {
			fclose(rfd);
			fprintf(stderr, "local read error on %s\n", local);
			CLOSE_DISCONNECT_AND_RETURN(client, wfd, -1);
		}

			// EOF
		if (num_read == 0) {
			break;
		}

		int num_written = chirp_client_write(client, wfd, buf, num_read);
		if (num_written != num_read) {
			fclose(rfd);
			fprintf(stderr, "Couldn't chirp_write as much as we read\n");
			CLOSE_DISCONNECT_AND_RETURN(client, wfd, -1);
		}

	} while (num_read > 0);
	::fclose(rfd);
		
	CLOSE_DISCONNECT_AND_RETURN(client, wfd, 0);
}

// New version using putfile
int
chirp_put_one_file(char *local, char *remote, int perm) {
	struct chirp_client *client;

	CONNECT_STARTER(client);

	FILE *rfd = ::safe_fopen_wrapper_follow(local, "rb");


	if (rfd == NULL) {
		fprintf(stderr, "Can't open local file %s\n", local);
		DISCONNECT_AND_RETURN(client, -1);
	}
	
		// Get size of file, allocate buffer
#if defined(WIN32)
	struct _stat stat_buf;
	if (_fstat(fileno(rfd), &stat_buf) == -1) {
#else
	struct stat stat_buf;
	if (fstat(fileno(rfd), &stat_buf) == -1) {
#endif
		fprintf(stderr, "Can't fstat local file %s\n", local);
		fclose(rfd);
		DISCONNECT_AND_RETURN(client, -1);
	}

	int size = stat_buf.st_size;
	char* buf = (char*)malloc(size);
	if ( ! buf) {
		fprintf(stderr, "Can't allocate %d bytes\n", size);
		fclose(rfd);
		DISCONNECT_AND_RETURN(client, -1);
	}
	
	int num_read = ::fread(buf, 1, size, rfd);
	
	if (num_read < 0) {
		fclose(rfd);
		free(buf);
		fprintf(stderr, "Local read error on %s\n", local);
		DISCONNECT_AND_RETURN(client, -1);
	}
	
		// Call putfile
	int num_written = chirp_client_putfile_buffer(client, remote, buf, perm, num_read);
	if (num_written != num_read) {
		fclose(rfd);
		free(buf);
		fprintf(stderr, "Couldn't chirp_write as much as we read\n");
		DISCONNECT_AND_RETURN(client, -1);
	}
	
	fclose(rfd);
	free(buf);
	DISCONNECT_AND_RETURN(client, 0);
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
	char default_mode [] = "cwb";
	char *mode = default_mode;
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
	if(strcmp(mode, "cwb") == 0 && strcmp(argv[fileOffset], "-") != 0) {
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
	CONNECT_STARTER(client);

    int rval = chirp_client_unlink(client, argv[2]);
	DISCONNECT_AND_RETURN(client, rval);
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
	CONNECT_STARTER(client);

	char *p = 0;
	int len = chirp_client_get_job_attr(client, argv[2], &p);
	printf("%.*s\n", len, p);
	free(p);
	DISCONNECT_AND_RETURN(client, 0);
}

int chirp_get_job_attr_delayed(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp get_job_attr_delayed AttributeName\n");
		printf("This retrieves the attribute value from the local starter.\n");
		printf("While this has no impact on the schedd, the attribute value may\n");
		printf("differ from the current one in the schedd, depending when the last\n");
		printf("schedd->starter update occurred.\n");
		return -1;
	}

	struct chirp_client *client = NULL;
	CONNECT_STARTER(client);

	char *p = 0;
	int len = chirp_client_get_job_attr_delayed(client, argv[2], &p);
	printf("%.*s\n", len, p);
	free(p);
	DISCONNECT_AND_RETURN(client, 0);
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
	CONNECT_STARTER(client);

    int rval = chirp_client_set_job_attr(client, argv[2], argv[3]);
	DISCONNECT_AND_RETURN(client, rval);
}

/*
 * chirp_set_job_attr_delayed
 * Do an update to the remote shadow.  The update is non-durable - it may be lost by the
 * schedd, but it will consume less resources on the submit side.
 */

int chirp_set_job_attr_delayed(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp set_job_attr_delayed AttributeName AttributeValue\n");
		printf("This will perform a non-durable update to the shadow; it consumes\n");
		printf("less resources than set_job_attr, but may be lost.\n");
		return -1;
	}

	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	int rval = chirp_client_set_job_attr_delayed(client, argv[2], argv[3]);
	DISCONNECT_AND_RETURN(client, rval);
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
	CONNECT_STARTER(client);

    int rval = chirp_client_ulog(client, argv[2]);
	DISCONNECT_AND_RETURN(client, rval);
}

/*
 * chirp_phase
 *
 */

int chirp_phase(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp phase phasestring\n");
		return -1;
	}

	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

    int rval = chirp_client_phase(client, argv[2]);
	DISCONNECT_AND_RETURN(client, rval);
}

int chirp_read(int argc, char **argv) {
	int fileOffset = 2;
	int offset = 0;
	int stride_length = 0;
	int stride_skip = 0;

	bool more = true;
	while (more && fileOffset + 1 < argc) {

		if (strcmp(argv[fileOffset], "-offset") == 0) {
			offset = strtol(argv[fileOffset + 1], NULL, 10);
			fileOffset += 2;
			more = true;
		}
		else if (strcmp(argv[fileOffset], "-stride") == 0
					&& fileOffset + 2 < argc) {
			stride_length = strtol(argv[fileOffset + 1], NULL, 10);
			stride_skip = strtol(argv[fileOffset + 2], NULL, 10);
			fileOffset += 3;
			more = true;
		}
		else {
			more = false;
		}
	}

	if(fileOffset + 2 != argc) {
		printf("condor_chirp read [-offset offset] [-stride length skip] "
			"remotepath length\n");
		return -1;
	}

	char *path = argv[fileOffset];
	int length = strtol(argv[fileOffset + 1], NULL, 10);
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	int fd = chirp_client_open(client, path, "r", 0);
	if(fd < 0) {
		DISCONNECT_AND_RETURN(client, fd);
	}
	
	void* buf = malloc(length+1);
	if ( ! buf) {
		printf("failed to allocate %d bytes\n", length);
		CLOSE_DISCONNECT_AND_RETURN(client, fd, -1);
	}

	int ret_val = -1;
	// Use read
	if(offset == 0 && stride_length == 0 && stride_skip == 0) {
		ret_val = chirp_client_read(client, fd, buf, length);
	}
	// Use pread
	else if(offset != 0 && stride_length == 0 && stride_skip == 0) {
		ret_val = chirp_client_pread(client, fd, buf, length, offset);
	}
	// Use sread
	else {
		ret_val = chirp_client_sread(client, fd, buf, length, offset,
			stride_length, stride_skip);
	}

	if(ret_val >= 0) {
		char* to_print = (char*)buf;
		to_print[length] = '\0';
		printf("%s\n", to_print);
	}

	free(buf);
	CLOSE_DISCONNECT_AND_RETURN(client, fd, ret_val);
	
}

int chirp_write(int argc, char **argv) {
	int fileOffset = 2;
	int offset = 0;
	int stride_length = 0;
	int stride_skip = 0;
	
	bool more = true;
	while (more && fileOffset + 1 < argc) {

		if (strcmp(argv[fileOffset], "-offset") == 0) {
			offset = strtol(argv[fileOffset + 1], NULL, 10);
			fileOffset += 2;
			more = true;
		}
		else if (strcmp(argv[fileOffset], "-stride") == 0
					&& fileOffset + 2 < argc) {
			stride_length = strtol(argv[fileOffset + 1], NULL, 10);
			stride_skip = strtol(argv[fileOffset + 2], NULL, 10);
			fileOffset += 3;
			more = true;
		}
		else {
			more = false;
		}
	}
	int length = 0;
	bool backward_compat = false;
	if(fileOffset + 2 == argc) {
		//backward compat
		backward_compat = true;
		if(stride_length != 0 || stride_skip != 0) {
			length = stride_length;
		} else {
			length = 1024;
		}
	} else if(fileOffset + 3 == argc) {
		length = strtol(argv[fileOffset + 2], NULL, 10);
	} else {
		printf("condor_chirp write [-offset offset] [-stride length skip] "
			"remote_file local_file length %d %d\n",fileOffset , argc);
		return -1;
	}
	
	char *remote_file = argv[fileOffset];
	char *local_file = argv[fileOffset+1];
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	int num_read = 0, num_written = 0, add = 0;
	FILE *rfd;
	char *buf;

		// Use stdin or a local file
	if (strcmp(local_file, "-") == 0) {
		rfd = stdin;
	} else {
		rfd = ::safe_fopen_wrapper_follow(local_file, "rb");
		if (!rfd) {
			fprintf(stderr, "Can't open local file %s\n", local_file);
			DISCONNECT_AND_RETURN(client, -1);
		}
	}
	
		// Open the remote file
	int fd = chirp_client_open(client, remote_file, "w", 0);
	if(fd < 0) {
		fclose(rfd);
		DISCONNECT_AND_RETURN(client, fd);
	}
	
	buf = (char*)malloc(length+1);
	if ( ! buf) {
		errno = ENOMEM;
		CLOSE_DISCONNECT_AND_RETURN(client, fd, -1);
	}

	int ret_val = -1;
	// Use pwrite
	if(stride_length == 0 && stride_skip == 0) {
		//offset is remote offset so we don't read using the offset value
		num_read = ::fread(buf, 1, length, rfd);
		if (num_read < 0) {
			fclose(rfd);
			free((char*)buf);
			fprintf(stderr, "Local read error on %s\n", local_file);
			CLOSE_DISCONNECT_AND_RETURN(client, fd, -1);
		}
		ret_val = chirp_client_pwrite(client, fd, buf, num_read, offset);
	} else {
		if (!backward_compat){
			num_read = ::fread(buf, 1, length, rfd);
			if (num_read < 0) {
				fclose(rfd);
				free((char*)buf);
				fprintf(stderr, "Local read error on %s\n", local_file);
				CLOSE_DISCONNECT_AND_RETURN(client, fd, -1);
			}
		
			ret_val = chirp_client_swrite(client, fd, buf, length, offset,
				stride_length, stride_skip);	
		} else {
			unsigned int total = 0;
			do {
				num_read = ::fread(buf, 1, length, rfd);
				if (num_read < 0) {
					fclose(rfd);
					free((char*)buf);
					fprintf(stderr, "Local read error on %s\n", local_file);
					CLOSE_DISCONNECT_AND_RETURN(client, fd, -1);
				}
	
				// EOF
				if (num_read == 0) {
					break;
				}
		
				// Use pwrite
				num_written = chirp_client_pwrite(client, fd, buf, num_read, offset+add);
	
				// Make sure we wrote the expected number of bytes
				if(num_written != num_read) {
					fclose(rfd);
					free((char*)buf);
					fprintf(stderr, "pwrite unable to write %d bytes\n", num_read);
					CLOSE_DISCONNECT_AND_RETURN(client, fd, -1);
				}
				total += num_written;
				if(stride_length != 0 || stride_skip != 0) {
					add += stride_skip;
				} else {
					add += num_read;
				}
			} while (num_read > 0);
			ret_val = total;
		}
	}
	fclose(rfd);
	free((char*)buf);
	CLOSE_DISCONNECT_AND_RETURN(client, fd, ret_val);
}

int chirp_rmdir(int argc, char **argv) {
	if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[2], "-r") != 0)) {
		printf("condor_chirp rmdir [-r] remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);
	
	int status = -1;
	
		// Use rmall if '-r' specified
	if(argc == 4) {
		status = chirp_client_rmall(client, argv[3]);
	} else {
		status = chirp_client_rmdir(client, argv[2]);
	}
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_getdir(int argc, char **argv) {
	if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[2], "-l") != 0)) {
		printf("condor_chirp getdir [-l] remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);
	
	char *buffer = NULL;
	int status = -1;
		
		// Use getlongdir if '-l' specified
	if(argc == 4) {
		if((status = chirp_client_getlongdir(client, argv[3], &buffer)) >= 0) {
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
		}
	} else {
		if((status = chirp_client_getdir(client, argv[2], &buffer) >= 0)) {
			printf("%s", buffer);
		}
	}
	free(buffer);
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_whoami() {
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);
	
	char* buffer = (char*)malloc(CHIRP_LINE_MAX);
	int status = chirp_client_whoami(client, buffer, CHIRP_LINE_MAX);
	printf("%s\n", buffer);
	free(buffer);
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_whoareyou(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp whoareyou remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);
	
	char* buffer = (char*)malloc(CHIRP_LINE_MAX);
	int status = chirp_client_whoareyou(client, argv[2], buffer, CHIRP_LINE_MAX);
	printf("%s\n", buffer);
	free(buffer);
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_link(int argc, char **argv) {
	if (argc < 4 || argc > 5 || (argc == 5 && strcmp(argv[2], "-s") != 0)) {
		printf("condor_chirp link [-s] oldpath newpath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	int status = -1;
	if(argc == 5) {
		status = chirp_client_symlink(client, argv[3], argv[4]);
	} else {
		status = chirp_client_link(client, argv[2], argv[3]);
	}
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_readlink(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp readlink remotepath length\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	char *buffer = NULL;

	int length = atoi(argv[3]);
	int status = chirp_client_readlink(client, argv[2], length, &buffer);
	if(status >= 0) {
		printf("%s\n", buffer);
	}
	free(buffer);
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_do_stat(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp %s remotepath\n", argv[1]);
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);
	
	struct chirp_stat stat;
	int status = -1;
	if(strcmp(argv[1], "lstat") == 0) {
		status = chirp_client_lstat(client, argv[2], &stat);
	} else {
		status = chirp_client_stat(client, argv[2], &stat);
	}
	if(status >= 0) {
		print_stat(&stat);
	}
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_do_statfs(int argc, char **argv) {
	if (argc != 3) {
		printf("condor_chirp statfs remotepath\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	struct chirp_statfs statfs;
	int status = chirp_client_statfs(client, argv[2], &statfs);
	if(status >= 0) {
		print_statfs(&statfs);
	}
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_access(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp access remotepath mode(rwxf)\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

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
				DISCONNECT_AND_RETURN(client, -1);
				break;
		}
		m++;
	}
	int status = chirp_client_access(client, argv[2], mode);
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_chmod(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp chmod remotepath mode\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	unsigned mode;
	if (1 != sscanf(argv[3], "%o", &mode)) {
		free(client);
		return EINVAL;
	}
	int status = chirp_client_chmod(client, argv[2], mode);
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_chown(int argc, char **argv) {
	if (argc != 5) {
		printf("condor_chirp %s remotepath uid gid\n", argv[1]);
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	int uid = atoi(argv[3]);
	int gid = atoi(argv[4]);
	int status = -1;
	if(strcmp(argv[1], "lchown")) {
		status = chirp_client_lchown(client, argv[2], uid, gid);
	} else {
		status = chirp_client_chown(client, argv[2], uid, gid);
	}
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_truncate(int argc, char **argv) {
	if (argc != 4) {
		printf("condor_chirp truncate remotepath length\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);

	int len = atoi(argv[3]);
	int status = chirp_client_truncate(client, argv[2], len);
	DISCONNECT_AND_RETURN(client, status);
}

int chirp_utime(int argc, char **argv) {
	if (argc != 5) {
		printf("condor_chirp utime remotepath actime mtime\n");
		return -1;
	}
	
	struct chirp_client *client = 0;
	CONNECT_STARTER(client);
	
	int actime = atoi(argv[3]);
	int mtime = atoi(argv[4]);
	int status = chirp_client_utime(client, argv[2], actime, mtime);
	DISCONNECT_AND_RETURN(client, status);
}

void usage() {
	printf("Usage:\n");
	printf("condor_chirp fetch remote_file local_file\n");
	printf("condor_chirp put [-mode mode] [-perm perm] local_file "
	   "remote_file\n");
	printf("condor_chirp remove remote_file\n");
	printf("condor_chirp get_job_attr job_attribute\n");
	printf("condor_chirp get_job_attr_delayed job_attribute\n");
	printf("condor_chirp set_job_attr job_attribute attribute_value\n");
	printf("condor_chirp set_job_attr_delayed job_attribute attribute_value\n");
	printf("condor_chirp ulog text\n");
	printf("condor_chirp phase phasestring\n");
	printf("condor_chirp read [-offset offset] [-stride length skip] "
		"remote_file length\n");
	printf("condor_chirp write [-offset remote_offset] [-stride length skip] "
		"remote_file local_file\n");
	printf("condor_chirp rmdir [-r] remotepath\n");
	printf("condor_chirp getdir [-l] remotepath\n");
	printf("condor_chirp whoami\n");
	printf("condor_chirp whoareyou remotepath\n");
	printf("condor_chirp link [-s] oldpath newpath\n");
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

	if (argc == 1 || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
		usage();
		exit(-1);
	}

#ifdef WIN32
	// defeat the windows abort message and/or dialog box. and also the crash dump
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
	// catch abort, and exit with code 1 instead.
	struct sigaction act;
	act.sa_handler = abort_handler;
	act.sa_flags = 0;
	sigfillset(&act.sa_mask);
	sigaction(SIGABRT,&act,0);
#endif

	if (strcmp("fetch", argv[1]) == 0) {
		ret_val = chirp_fetch(argc, argv);
	} else if (strcmp("put", argv[1]) == 0) {
		ret_val = chirp_put(argc, argv);
	} else if (strcmp("remove", argv[1]) == 0) {
		ret_val = chirp_remove(argc, argv);
	} else if (strcmp("get_job_attr_delayed", argv[1]) == 0) {
		ret_val = chirp_get_job_attr_delayed(argc, argv);
	} else if (strcmp("get_job_attr", argv[1]) == 0) {
		ret_val = chirp_get_job_attr(argc, argv);
	} else if (strcmp("set_job_attr_delayed", argv[1]) == 0) {
		ret_val = chirp_set_job_attr_delayed(argc, argv);
	} else if (strcmp("set_job_attr", argv[1]) == 0) {
		ret_val = chirp_set_job_attr(argc, argv);
	} else if (strcmp("ulog", argv[1]) == 0) {
		ret_val = chirp_ulog(argc, argv);
	} else if (strcmp("phase", argv[1]) == 0) {
		ret_val = chirp_phase(argc, argv);
	} else if (strcmp("read", argv[1]) == 0) {
		ret_val = chirp_read(argc, argv);
	} else if (strcmp("write", argv[1]) == 0) {
		ret_val = chirp_write(argc, argv);
	} else if (strcmp("rmdir", argv[1]) == 0) {
		ret_val = chirp_rmdir(argc, argv);	
	} else if (strcmp("getdir", argv[1]) == 0) {
		ret_val = chirp_getdir(argc, argv);
	} else if (strcmp("whoami", argv[1]) == 0) {
		ret_val = chirp_whoami();
	} else if (strcmp("whoareyou", argv[1]) == 0) {
		ret_val = chirp_whoareyou(argc, argv);
	} else if (strcmp("link", argv[1]) == 0) {
		ret_val = chirp_link(argc, argv);
	} else if (strcmp("readlink", argv[1]) == 0) {
		ret_val = chirp_readlink(argc, argv);
	} else if (strcmp("stat", argv[1]) == 0 || strcmp("lstat", argv[1]) == 0) {
		ret_val = chirp_do_stat(argc, argv);
	} else if (strcmp("statfs", argv[1]) == 0) {
		ret_val = chirp_do_statfs(argc, argv);
	} else if (strcmp("access", argv[1]) == 0) {
		ret_val = chirp_access(argc, argv);
	} else if (strcmp("chmod", argv[1]) == 0) {
		ret_val = chirp_chmod(argc, argv);
	} else if (strcmp("chown", argv[1]) == 0 || strcmp("lchown", argv[1]) == 0){
		ret_val = chirp_chown(argc, argv);
	} else if (strcmp("truncate", argv[1]) == 0) {
		ret_val = chirp_truncate(argc, argv);
	} else if (strcmp("utime", argv[1]) == 0) {
		ret_val = chirp_utime(argc, argv);
	} else {
		printf("Unknown command %s\n", argv[1]);
		usage();
		exit(-1);
	}

	if(ret_val < 0 && errno != 0) {
		printf("\tError: %d (%s)\n", errno, strerror(errno));
	}
	return ret_val;
}
