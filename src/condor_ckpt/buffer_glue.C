
#include "buffer_glue.h"
#include "image.h"

static BufferCache *Buffer=0;
static BufferIsConfigured=0;
static BufferPrefetchSize=0;

/** This shuts down buffering as the program exits */

static void BufferGlueShutdown()
{
	if( BufferIsConfigured && Buffer ) {
		Buffer->flush();
		Buffer=0;
	}
}

void BufferGlueConfigure()
{
	int status, blocks, block_size;

	if( BufferIsConfigured ) return;

        BufferIsConfigured = 1;

        dprintf(D_ALWAYS, "BufferGlueConfigure: Getting buffer info\n");
	status = REMOTE_syscall(CONDOR_get_buffer_info, &blocks, &block_size, &BufferPrefetchSize );

        if (status < 0) {
                dprintf(D_ALWAYS,"BufferGlueConfigure: Failed to get info.\n");
        } else {
                dprintf(D_ALWAYS,"BufferGlueConfigure: %d blocks of %d bytes, prefetch %d bytes.\n", blocks, block_size, BufferPrefetchSize);
                if (blocks * block_size > 0) {
                        Buffer = new BufferCache (blocks, block_size);
                        if (!Buffer)
                                dprintf(D_ALWAYS,"BufferGlueConfigure: Unable to create buffer!\n");
                } else {
                        dprintf(D_ALWAYS,"BufferGlueConfigure: User did not request buffering.\n");
                }
        }

	atexit(BufferGlueShutdown);
}

int BufferGlueActive( File *f )
{
	return BufferIsConfigured && Buffer && f && f->isBufferable();
}

ssize_t BufferGlueRead( File *f, char *data, size_t nbytes )
{
	int offset = f->getOffset();
	int size = f->getSize();
	int actual;

	// Check readability

	if( !f->isReadable() ) {
		errno = EBADF;
		return -1;
	}

	// Check seek pointer boundaries

	if( (offset>size) || (nbytes==0) ) return 0;

	// The buffer doesn't know about file sizes, so cut off
	// extra long reads here

	if( (offset+nbytes)>size ) nbytes = size-offset;

	// Consult the buffer

	actual = Buffer->read( f, offset, data, nbytes );

	// Update the seek pointer unless there was an error

	if(actual<0) return -1;

	f->setOffset(offset+actual);

	return actual;
}

ssize_t BufferGlueWrite( File *f, char *data, size_t nbytes )
{
	int offset = f->getOffset();
	int size = f->getSize();
	int actual;

	if(!f->isWriteable()) {
		errno = EBADF;
		return -1;
	}

	if( (!data) || (nbytes<0) ) {
		errno = EINVAL;
		return -1;
	}

	if( nbytes==0 ) return 0;

	actual = Buffer->write( f, offset, data, nbytes );

	if(actual<0) return -1;

	if( (offset+actual) > size )
		f->setSize(offset+actual);

	f->setOffset(offset+actual);

	return actual;
}

off_t BufferGlueSeek( File *f, off_t offset, int whence )
{
	off_t temp;

        // Compute the new offset first.
        // If the new offset is illegal, don't change it.

        if( whence == SEEK_SET ) {
                temp = offset;
        } else if( whence == SEEK_CUR ) {
                temp = f->getOffset()+offset;
        } else if( whence == SEEK_END ) {
                temp = f->getSize()+offset;
        } else {
                errno = EINVAL;
                return -1;
        }

        if(temp<0) {
                errno = EINVAL;
                return -1;
        } else {
                f->setOffset(temp);
                return temp;
        }
}

void BufferGlueOpenHook( File *f )
{
	// Prefetch data if necessary
	if( Buffer ) Buffer->prefetch( f, 0, BufferPrefetchSize );
}

void BufferGlueCloseHook( File *f )
{
	// Flush or invalidate all data associated with this file.
	if(Buffer) Buffer->flush(f);
}

extern "C" void _condor_file_warning( char *format, ... )
{
        va_list args;
        va_start(args,format);

        static char text[1024];
        vsprintf(text,format,args);

        if(MyImage.GetMode()==STANDALONE) {
                fprintf(stderr,"Condor Warning: %s\n",text);
        } else if(LocalSysCalls()) {
                dprintf(D_ALWAYS,"Condor Warning: %s\n",text);
        } else {
                REMOTE_syscall( CONDOR_perm_error, text );
        }

        va_end(args);
}



