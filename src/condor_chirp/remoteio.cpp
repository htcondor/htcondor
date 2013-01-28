
#define FUSE_USE_VERSION  26

#include <stdlib.h>  
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

#include <pthread.h>

#include "chirp_client.h"

pthread_mutex_t op_mutex = PTHREAD_MUTEX_INITIALIZER;

struct LockGuard {
	LockGuard() {pthread_mutex_lock(&op_mutex);}
	~LockGuard() {pthread_mutex_unlock(&op_mutex);}
};

#define GET_CLIENT(client) \
	LockGuard sentry; \
	struct chirp_client *client = (struct chirp_client*)fuse_get_context()->private_data; \
	if (!client) return -EIO

#define COPY_CHIRP_TO_STAT(cstat, stbuf) \
	stbuf->st_dev = cstat.cst_dev; \
	stbuf->st_ino = cstat.cst_ino; \
	stbuf->st_mode = cstat.cst_mode; \
	stbuf->st_nlink = cstat.cst_nlink; \
	stbuf->st_uid = cstat.cst_uid; \
	stbuf->st_gid = cstat.cst_gid; \
	stbuf->st_rdev = cstat.cst_rdev; \
	stbuf->st_size = cstat.cst_size; \
	stbuf->st_blksize = cstat.cst_blksize; \
	stbuf->st_blocks = cstat.cst_blocks; \
	stbuf->st_atime = cstat.cst_atime; \
	stbuf->st_mtime = cstat.cst_mtime; \
	stbuf->st_ctime = cstat.cst_ctime

#define COPY_CHIRP_TO_STATVFS(cstat, stbuf) \
	/*stbuf->f_type = cstat.f_type;*/ \
	stbuf->f_bsize = cstat.f_bsize; \
	stbuf->f_blocks = cstat.f_blocks; \
	stbuf->f_bfree = cstat.f_bfree; \
	stbuf->f_bavail = cstat.f_bavail; \
	stbuf->f_files = cstat.f_files; \
	stbuf->f_ffree = cstat.f_ffree \

// Definitions for FUSE option parsing
typedef struct options_s {
	char *keyfile;
	int nocheck;
} options_t;
options_t options;

static int chirp_getattr(const char *path, struct stat *stbuf) {
	GET_CLIENT(client);
	struct chirp_stat cstat;
	int ret = chirp_client_stat(client, path, &cstat);
	if (ret) {
		printf("Attr result: %d.\n", ret);
		return -ENOENT;
	}
	COPY_CHIRP_TO_STAT(cstat, stbuf);
	return 0;
}

static void * chirp_init(struct fuse_conn_info * conn) {
	if (conn) {}
	struct chirp_client *client;
	if (options.keyfile) {
		const char *path_part;
		client = chirp_client_connect_url(options.keyfile, &path_part);
	} else {
		client = chirp_client_connect_default();
	}
	return client;
}

static void chirp_destroy(void * arg) {
	struct chirp_client *client = (struct chirp_client *)arg;
	if (client)
		chirp_client_disconnect(client);
}

static int chirp_readdir(const char *path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	if (offset) {}
	if (fi) {}
	GET_CLIENT(client);

	int res = 0;
	char *buffer;
	if ((res = chirp_client_getlongdir(client, path, &buffer)) < 0) {
		return -EIO;
	}

	char * line = strtok(buffer, "\n");
	while(line != NULL) {
		struct chirp_stat cstat;
		char * filename = line;
		line = strtok(NULL, "\n");
		if (line == NULL || get_stat(line, &cstat) < 0) {
			res = -EIO;
			break;
		}
		line = strtok(NULL, "\n");
		res = 0;
		struct stat stbuf; struct stat *stptr = &stbuf;
		COPY_CHIRP_TO_STAT(cstat, stptr);
		if ((res = filler(buf, filename, &stbuf, 0)) != 0) {
			res = -EIO;
			break;
		}
	}

	const char *const dots [] = { ".",".."};
	int i;
	for (i = 0 ; i < 2 ; i++) {
		struct stat st;
		memset(&st, 0, sizeof(struct stat));

		// set to 0 to indicate not supported for directory because we cannot (efficiently) get this info for every subdirectory
		st.st_nlink =  0;

		// setup stat size and acl meta data
		st.st_size    = 512;
		st.st_blksize = 512;
		st.st_blocks  =  1;
		st.st_mode    = (S_IFDIR | 0777);
		st.st_uid     = 0;
		st.st_gid     = 0;
		// todo fix below times
		st.st_atime   = 0;
		st.st_mtime   = 0;
		st.st_ctime   = 0;

		const char *const str = dots[i];

		// flatten the info using fuse's function into a buffer
		int res = 0;
		if ((res = filler(buf,str,&st,0)) != 0) {
			res = -EIO;
			break;
		}
	}
	return res;
}

static int chirp_open(const char * path, struct fuse_file_info * fi) {
	GET_CLIENT(client);

	char flags[7]; memset(flags, 0, 7);
	unsigned flag_val=0;
	if (fi->flags & O_RDWR) {
		flags[flag_val++] = 'r';
		flags[flag_val++] = 'w';
	}
	else if (fi->flags & O_WRONLY) {
		flags[flag_val++] = 'w';
	}
	else {
		flags[flag_val++] = 'r';
	}
	if (fi->flags & O_APPEND) {
		flags[flag_val++] = 'a';
	}
	if (fi->flags & O_TRUNC) {
		flags[flag_val++] = 't';
	}
	if (fi->flags & O_CREAT) {
		flags[flag_val++] = 'c';
	}
	if (fi->flags & O_EXCL) {
		flags[flag_val++] = 'x';
	}

	int res = 0, fh;
	printf("path %s; flags %s.\n", path, flags);
	if ((fh = chirp_client_open(client, path, flags, 0)) < 0) {
		res = -EIO;
	} else {
		fi->fh = fh;
	}
	return res;
}

static int chirp_close(const char * path, struct fuse_file_info *fi) {
	GET_CLIENT(client);

	assert(path);
	return chirp_client_close(client, fi->fh);
}

static int chirp_read(const char * path, char * buffer, size_t size, off_t offset, struct fuse_file_info * fi) {
	GET_CLIENT(client);
	assert(path);
	return chirp_client_pread(client, fi->fh, buffer, size, offset);
}

static int chirp_access(const char * path, int mode) {
	GET_CLIENT(client);
	return chirp_client_access(client, path, mode);
}

static int chirp_symlink(const char * path, const char * newpath) {
	GET_CLIENT(client);
	return chirp_client_symlink(client, path, newpath);
}

static int chirp_statfs(const char * path, struct statvfs* stbuf) {
	GET_CLIENT(client);
	struct chirp_statfs cstatfs;
	int ret = chirp_client_statfs(client, path, &cstatfs);
	if (ret == 0)
		COPY_CHIRP_TO_STATVFS(cstatfs, stbuf);
	return ret;
}

static int chirp_mkdir(const char * path, mode_t mode) {
	GET_CLIENT(client);
	return chirp_client_mkdir(client, path, mode);
}

static int chirp_rmdir(const char * path) {
	GET_CLIENT(client);
	return chirp_client_rmdir(client, path);
}

static int chirp_rename(const char * path, const char * newpath) {
	GET_CLIENT(client);
	return chirp_client_rename(client, path, newpath);
}

static int chirp_unlink(const char * path) {
	GET_CLIENT(client);
	return chirp_client_unlink(client, path);
}

static int chirp_write(const char * path, const char * buf, size_t size, off_t off, struct fuse_file_info *fi) {
	GET_CLIENT(client);
	assert(path);
	return chirp_client_pwrite(client, fi->fh, buf, size, off);
}

static int chirp_utimens(const char * path, const struct timespec tv[2]) {
	GET_CLIENT(client);
	return chirp_client_utime(client, path, tv[0].tv_sec, tv[1].tv_sec);
}

static int chirp_chmod(const char * path, mode_t mode) {
	GET_CLIENT(client);
	return chirp_client_chmod(client, path, mode);
}

static int chirp_chown(const char * path, uid_t uid, gid_t gid) {
	GET_CLIENT(client);
	return chirp_client_chown(client, path, uid, gid);
}

static int chirp_truncate(const char * path, off_t off) {
	GET_CLIENT(client);
	return chirp_client_truncate(client, path, off);
}

static int chirp_create(const char *path, mode_t mode, struct fuse_file_info * fi) {
	GET_CLIENT(client);
	int fd = chirp_client_open(client, path, "rwc", mode);
	if (fd < 0) {
		return -EIO;
	}
	fi->fh = fd;
	return 0;
}

// Specify the mappings from POSIX operations to our implementations.
static struct fuse_operations chirp_oper;

enum {
	KEY_VERSION,
	KEY_HELP,
};
static struct fuse_opt chirp_opts[] = {
	{"keyfile=%s", offsetof(options_t, keyfile), 0},
	{"nocheck", offsetof(options_t, nocheck), 1},
	FUSE_OPT_KEY("-V",             KEY_VERSION),
	FUSE_OPT_KEY("--version",      KEY_VERSION),
	FUSE_OPT_KEY("-h",             KEY_HELP),
	FUSE_OPT_KEY("--help",         KEY_HELP),
	{0, 0, 0}
};

static int chirp_opts_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
	assert(data);
	assert(arg);
	switch (key) {
	case KEY_HELP:
		fprintf(stderr,
			"usage: %s mountpoint [options]\n"
			"\n"
			"general options:\n"
			"    -o opt,[opt...]  mount options\n"
			"    -h   --help      print help\n"
			"    -V   --version   print version\n"
			"\n"
			"Chirp options:\n"
			"    -o keyfile=PATH	Location of chirp server locator file\n"
			"    -o nocheck		Do not check for chirp connectivity prior to daemonizing\n"
			"\n"
		, outargs->argv[0]);
		fuse_opt_add_arg(outargs, "-ho");
		fuse_main(outargs->argc, outargs->argv, &chirp_oper, NULL);
		exit(1);

	case KEY_VERSION:
		fprintf(stderr, "Chirp remote IO version 7.9.4\n");
		fuse_opt_add_arg(outargs, "--version");
		fuse_main(outargs->argc, outargs->argv, &chirp_oper, NULL);
		exit(0);
     }
     return 1;
}

int main(int argc, char *argv[]) 
{
	chirp_oper.getattr   = chirp_getattr;
	chirp_oper.readdir   = chirp_readdir;
	chirp_oper.open      = chirp_open;
	chirp_oper.release   = chirp_close;
	chirp_oper.read      = chirp_read;
	chirp_oper.init      = chirp_init;
	chirp_oper.destroy   = chirp_destroy;
	chirp_oper.access    = chirp_access;
	chirp_oper.symlink   = chirp_symlink;
	chirp_oper.statfs    = chirp_statfs;
	chirp_oper.mkdir     = chirp_mkdir;
	chirp_oper.rmdir     = chirp_rmdir;
	chirp_oper.rename    = chirp_rename;
	chirp_oper.unlink    = chirp_unlink;
	chirp_oper.write     = chirp_write;
	chirp_oper.utimens   = chirp_utimens;
	chirp_oper.chmod     = chirp_chmod;
	chirp_oper.chown     = chirp_chown;
	chirp_oper.truncate  = chirp_truncate;
	chirp_oper.create    = chirp_create;

	// Parse options.
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	memset(&options, 0, sizeof(options_t));
	if (fuse_opt_parse(&args, &options, chirp_opts, chirp_opts_proc) == -1)
		return -1;

	int ret = 0;
	if (!options.nocheck) {
		struct chirp_client *client = NULL;
		if (options.keyfile) {
			const char *path_part;
			client = chirp_client_connect_url(options.keyfile, &path_part);
			if (!client) 
				printf("Failed to connect to the chirp server using URL %s.", options.keyfile);
		} else {
			client = chirp_client_connect_default();
			if (!client) 
				printf("Failed to connect to the chirp server.");
		}
		if (!client) {
			ret = 1;
		} else {
			chirp_client_disconnect(client);
		}
	}

	if (!ret) ret = fuse_main(args.argc, args.argv, &chirp_oper, NULL);
	if (ret) printf("\n");

	fuse_opt_free_args(&args);

	return ret;
}       

