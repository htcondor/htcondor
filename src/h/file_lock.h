#ifndef FILE_LOCK_H
#define FILE_LOCK_H

typedef enum { READ_LOCK, WRITE_LOCK, UN_LOCK } LOCK_TYPE;

#if defined(__cplusplus)

class FileLock {
public:
	FileLock( int fd );
	~FileLock();

		// Read only access functions
	BOOLEAN		is_blocking();		// whether or not operations will block
	LOCK_TYPE	get_state();		// cur state READ_LOCK, WRITE_LOCK, UN_LOCK
	void		display();

		// Control functions
	void		set_blocking( BOOLEAN );	// set blocking TRUE or FALSE
	BOOLEAN		obtain( LOCK_TYPE t );		// get a lock
	BOOLEAN		release();					// release a lock

private:
	int			fd;			// File descriptor to deal with
	BOOLEAN		blocking;	// Whether or not to block
	LOCK_TYPE	state;		// Type of lock we are holding
};

#endif	/* cpluscplus */

#endif
