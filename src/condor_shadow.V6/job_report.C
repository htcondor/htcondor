
#include "job_report.h"
#include "syscall_numbers.h"
#include "condor_sys.h"
#include "pseudo_ops.h"

/* An array to store a count of all system calls */

#define SYSCALL_COUNT_SIZE (CONDOR_SYSCALL_MAX-CONDOR_SYSCALL_MIN)+1
static int syscall_counts[SYSCALL_COUNT_SIZE] = {0};

/* Record the execution of one system call. */

int job_report_store_call( int call )
{
	if( call>CONDOR_SYSCALL_MAX ) return -1;
	if( call<CONDOR_SYSCALL_MIN ) return -1;

	return ++syscall_counts[call-CONDOR_SYSCALL_MIN];
}

/* Display a list of all the calls made */

void job_report_display_calls( FILE *f )
{
	int i;

	fprintf(f,"\nRemote System Calls:\n");

	for( i=0; i<SYSCALL_COUNT_SIZE; i++ ) {
		if(syscall_counts[i]) {
			fprintf(f,"\t%-30s %5d\n",
				_condor_syscall_name(i+CONDOR_SYSCALL_MIN),
				syscall_counts[i]);
		}
	}
}

/* A linked list for storing text reports */

struct error_node {
	char *text;
	struct error_node *next;
};

static struct error_node *error_head=0;

/* Record a line of error information about the job */

int job_report_store_error( char *format, ... )
{
	struct error_node *e;

	va_list args;
	va_start( args, format );

	/* Create a node containing the text */
	
	e = (struct error_node *) malloc(sizeof(struct error_node));
	if(!e) return 0;

	e->text = (char *) malloc(JOB_REPORT_RECORD_MAX);
	if(!e->text) {
		free(e);
		return 0;
	}
	vsprintf( e->text, format, args );

	e->next = error_head;
	error_head = e;

	va_end( args );

	return 1;
}

/* Display the list of stored errors */

void job_report_display_errors( FILE *f )
{
	struct error_node *i;

	if( error_head ) {
		fprintf( f, "*** ERRORS:\n");

		for( i=error_head; i; i=i->next ) {
			fprintf(f,"\t* %s\n",i->text);
		}
	}
}


/* A linked list for storing file reports */

struct file_info {
	char kind[_POSIX_PATH_MAX];
	char name[_POSIX_PATH_MAX];
	int open_count;
	int read_count, write_count, seek_count;
	int read_bytes, write_bytes;
	struct file_info *next;
};

static struct file_info *file_list=0;

/* Store the current number of I/O ops performed */

void job_report_store_file_info( char *kind, char *name, int oc, int rc, int wc, int sc, int rb, int wb )
{
	struct file_info *i;

	if( !rc && !wc && !sc ) return;

	/* Has this file been opened and closed before? */

	for( i=file_list; i; i=i->next ) {
		if(!strcmp(i->name,name) && !strcmp(i->kind,kind)) {
			i->open_count = oc;
			i->read_count = rc;
			i->write_count = wc;
			i->seek_count = sc;
			i->read_bytes = rb;
			i->write_bytes = wb;
			return;
		}
	}

	/* If not, make a new node. */

	i = (struct file_info *) malloc(sizeof(struct file_info));
	if(!i) return;

	strcpy( i->kind, kind );
	strcpy( i->name, name );
	i->open_count = oc;
	i->read_count = rc;
	i->write_count = wc;
	i->seek_count = sc;
	i->read_bytes = rb;
	i->write_bytes = wb;

	i->next = file_list;
	file_list = i;
}

static void sum_file_info( struct file_info *total )
{
	struct file_info *i;

	total->open_count=0;
	total->read_count=0;
	total->write_count=0;
	total->seek_count=0;
	total->read_bytes=0;
	total->write_bytes=0;

	for( i=file_list; i; i=i->next ) {
		total->open_count += i->open_count;
		total->read_count += i->read_count;
		total->write_count += i->write_count;
		total->seek_count += i->seek_count;
		total->read_bytes += i->read_bytes;
		total->write_bytes += i->write_bytes;
	}
}

/* Display the total I/O ops performed */

void job_report_display_file_info( FILE *f, int total_time )
{
	struct file_info *i;
	struct file_info total;

	int	buffer_size;
	int	buffer_block_size;

	sum_file_info( &total );

	pseudo_get_buffer_info( &buffer_size, &buffer_block_size );
	
	fprintf(f,"\nBuffer Configuration:\n");
	fprintf(f,"\t%s max buffer space per open file\n",
		metric_units(buffer_size) );
	fprintf(f,"\t%s buffer block size\n",
		metric_units(buffer_block_size) );

	fprintf(f,"\nTotal I/O:\n\n");
	if( total_time>0 ) {
		fprintf(f,"\t%s/s effective throughput\n",
			metric_units((total.read_bytes+total.write_bytes)/total_time));
	}
	fprintf(f,"\t%d files opened\n", total.open_count );
	fprintf(f,"\t%d reads totaling %s\n",
		total.read_count, metric_units(total.read_bytes) );
	fprintf(f,"\t%d writes totaling %s\n",
		total.write_count, metric_units(total.write_bytes) );
	fprintf(f,"\t%d seeks\n", total.seek_count );

	fprintf(f,"\nI/O by File:\n");

	for( i=file_list; i; i=i->next ) {
		fprintf(f,"\n%s %s\n",i->kind,i->name);
		fprintf(f,"\topened %d times\n",i->open_count );
		fprintf(f,"\t%d reads totaling %s\n",
			i->read_count, metric_units(i->read_bytes) );
		fprintf(f,"\t%d writes totaling %s\n",
			i->write_count, metric_units(i->write_bytes) );
		fprintf(f,"\t%d seeks\n", i->seek_count );
	}
}

/* Send the stored info back to the Q */

void job_report_update_queue( PROC *proc )
{
	struct file_info total;

	sum_file_info( &total );

	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_READ_COUNT, total.read_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_READ_BYTES, total.read_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_WRITE_COUNT, total.write_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_WRITE_BYTES, total.write_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_SEEK_COUNT, total.seek_count );
}


