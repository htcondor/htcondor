#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

extern void ckpt_and_exit(void);

#if defined(NO_CONDOR)
void ckpt_and_exit(void) { }
#endif

void exit_success(void)
{
	printf("SUCCESS\n");
	fflush(NULL);
	exit(EXIT_SUCCESS);
}

void exit_failure(void)
{
	printf("FAILURE\n");
	fflush(NULL);
	exit(EXIT_FAILURE);
}

void delete_file(char *file)
{
	if (unlink(file) < 0) {
		if (errno != ENOENT) {
			printf("ERROR: unlinked failed: %d(%s)\n", errno, strerror(errno));
			exit_failure();
		}
	}
}

/* lifted from the condor source */
ssize_t
full_read(int fd, void *ptr, size_t nbytes)
{
	int nleft, nread;

	nleft = nbytes;
	while (nleft > 0) {

#ifndef WIN32
		REISSUE_READ: 
#endif
		nread = read(fd, ptr, nleft);
		if (nread < 0) {

#ifndef WIN32
			/* error happened, ignore if EINTR, otherwise inform the caller */
			if (errno == EINTR) {
				goto REISSUE_READ;
			}
#endif
			/* The caller has no idea how much was actually read in this
				scenario and the file offset is undefined */
			return -1;

		} else if (nread == 0) {
			/* We've reached the end of file marker, so stop looping. */
			break;
		}

		nleft -= nread;
			/* On Win32, void* does not default to "byte", so we cast it */
		ptr = ((char *)ptr) + nread;
	}

	/* return how much was actually read, which could include 0 in an
		EOF situation */
	return (nbytes - nleft);	 
}

/* lifted from the condor source */
ssize_t
full_write(int fd, const void *ptr, size_t nbytes)
{
	int nleft, nwritten;

	nleft = nbytes;
	while (nleft > 0) {
#ifndef WIN32
		REISSUE_WRITE: 
#endif
		nwritten = write(fd, ptr, nleft);
		if (nwritten < 0) {
#ifndef WIN32
			/* error happened, ignore if EINTR, otherwise inform the caller */
			if (errno == EINTR) {
				goto REISSUE_WRITE;
			}
#endif
			/* The caller has no idea how much was written in this scenario
				and the file offset is undefined. */
			return -1;
		}

		nleft -= nwritten;
			/* On Win32, void* does not default to "byte", so we cast it */
		ptr   = ((const char*) ptr) + nwritten;
	}
	
	/* return how much was actually written, which could include 0 */
	return (nbytes - nleft);
}


void fill_chunk(unsigned char *chunk, int len)
{
	int i;

	for(i = 0; i < len; i++) {
		chunk[i] = rand() % 256;
	}
}

void check_chunk(unsigned char *chunk, int len)
{
	int i;

	for(i = 0; i < len; i++) {
		if (chunk[i] != rand() % 256) {
			printf("ERROR: File did not contain same squence of data as when "
				"it was written!\n");
			exit_failure();
		}
	}
}

void write_a_file(char *file, off_t chunk_size, off_t num_chunks)
{
	int fd = -1;
	off_t i;
	unsigned char *chunk = NULL;
	off_t calculated_pos;
	off_t actual_pos;

	/* I want preproducability */
	srand(0);


	chunk = calloc(chunk_size, 1);
	if (chunk == NULL) {
		printf("ERROR: Out of memory!\n");
		exit_failure();
	}

	fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		printf("ERROR: Failed to open the file '%s' for writing. %d(%s)\n",
			file, errno, strerror(errno));
		exit_failure();
	}

	calculated_pos = 0;
	for (i = 0; i < num_chunks; i++) {
		fill_chunk(chunk, chunk_size);
		if (full_write(fd, chunk, chunk_size) != chunk_size) {
			printf("ERROR: Failed to write file: %d(%s)\n", 
				errno, strerror(errno));
			exit_failure();
		}
		calculated_pos += chunk_size;
		actual_pos = lseek(fd, (off_t)0, SEEK_CUR);

		/* we checkpoint at various points in the file */
		if (i == 0 || /* start */
			i == ((num_chunks >> 1) - (num_chunks >> 2)) || /* 25% */
			i == (num_chunks >> 1) || /* 50% */
			i == ((num_chunks >> 1) + (num_chunks >> 2)) || /* 75% */
			i == (num_chunks - 1)) /* end */
		{
			printf("Checkpointing at %.0lf%% of the file operation.\n", 
				i==0?0:((double) i / (double) num_chunks) * 100.0);
			printf("Skipping checkpoint!\n");
			ckpt_and_exit();
		}

		if (actual_pos != calculated_pos) {
			printf("ERROR: lseek() failed %d(%s) to get current position of "
				"writing file. Expected %lld. Got %lld.\n",
				errno, strerror(errno),
				(long long int) calculated_pos,
				(long long int) actual_pos);
			exit_failure();
		}
	}

	close(fd);

	free(chunk);

}

void check_a_file(char *file, off_t chunk_size, off_t num_chunks)
{
	int fd = -1;
	off_t i;
	unsigned char *chunk = NULL;
	off_t calculated_pos;
	off_t actual_pos;

	/* I need to generate the same sequence of random numbers when reading the
		file as when generating it. */
	srand(0);

	chunk = calloc(chunk_size, 1);
	if (chunk == NULL) {
		printf("ERROR: Out of memory!\n");
		exit_failure();
	}

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		printf("ERROR: Failed to open the file '%s' for verification. %d(%s)\n",
			file, errno, strerror(errno));
		exit_failure();
	}

	calculated_pos = 0;
	for (i = 0; i < num_chunks; i++) {
		if (full_read(fd, chunk, chunk_size) != chunk_size) {
			printf("ERROR: Failed to read file: %d(%s)\n",
				errno, strerror(errno));
			exit_failure();
		}
		check_chunk(chunk, chunk_size);

		calculated_pos += chunk_size;
		actual_pos = lseek(fd, (off_t)0, SEEK_CUR);

		/* we checkpoint at various points in the file */
		if (i == 0 || /* start */
			i == ((num_chunks >> 1) - (num_chunks >> 2)) || /* 25 % */
			i == (num_chunks >> 1) || /* 50% */
			i == ((num_chunks >> 1) + (num_chunks >> 2)) || /* 75% */
			i == (num_chunks - 1)) /* end */
		{
			printf("Checkpointing at %.0lf%% of the file operation.\n", 
				i==0?0:((double) i / (double) num_chunks) * 100.0);
			printf("Skipping checkpoint!\n");
			ckpt_and_exit();
		}

		if (actual_pos != calculated_pos) {
			printf("ERROR: lseek() failed %d(%s) to get current position of "
				"reading file. Expected %lld. Got %lld.\n",
				errno, strerror(errno),
				(long long int) calculated_pos,
				(long long int) actual_pos);
			exit_failure();
		}
	}

	close(fd);

	free(chunk);
}

int main(int argc, char *argv[])
{
	int gigs = 1;
	char file[1024] = {'\0'};

	if (argc == 2) {
		gigs = atoi(argv[1]);
		if (gigs == 0 || gigs < 0) {
			printf("Please supply a natural number of gigs.\n");
			exit_failure();
		}
	}

	sprintf(file, "./lfs-test.%u", (unsigned int)getpid());
	printf("sizeof int = %u\n", (unsigned int)sizeof(int));
	printf("sizeof off_t = %u\n", (unsigned int)sizeof(off_t));
	printf("sizeof long = %u\n", (unsigned int)sizeof(long));
	printf("sizeof long long = %u\n", (unsigned int)sizeof(long long));
	printf("Creating/Using file: %s\n", file);

	printf("Creating a %d gig file\n", gigs);
	write_a_file(file, 1024, (1024 * 1024) * gigs);

	printf("Verifying the %d gig file\n", gigs);
	check_a_file(file, 1024, (1024 * 1024) * gigs);

	delete_file(file);
	exit_success();
	return 0; /* to shut compiler up, never reached */
}
