/* 
** Copyright 1993 by Miron Livny, Mike Litzkow, and Emmanuel Ackaouy.
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Emmanuel Ackaouy
**
*/ 



/*
	* COMPONENTS:

	The condor stream library consists of (for now):
	- File streams (file.h)
	- InCore streams (incore.h)
	- Network socket streams (socks.h)
	   - Reliable network streams (on top of tcp)
	   - Safe network streams (on top of udp)


	* DATA REPRESENTATION:

	Streams can be in 3 modes:
	- Internal binary representation
	- External binary representation
	- Ascii representation	(not implemented)


	* CODING/DECODING:

	In the coding/decoding environment, there are 4 base objects:
	- char
	- int/long/short
	- float/double
	- null terminated (or plain null) strings


	* PROTOCOL:

	The Stream base class defines the private protocol for all Streams.
	This protocol consists of 10 virtual functions:

	1) Destructor()
		- Local destructor.

	2) stream_type type()
		- returns its type (file, incore, ...)

	3) int put_bytes(const void *dta, int s)
		- puts s bytes starting at dta in the stream.
		  returns the number actually put, or a negative value
		  on errors.

	4) int get_bytes(void *dta, int ms)
		- gets a maximum of ms bytes from the stream, in the memory
		  location starting at dta.
		  returns the actual number of bytes read, or a negative
		  value on errors.

	5) int get_ptr(void *&ptr, char d)
		- gets a temporary (non valid at next stream call) string with
		  delimiter d from the stream.
		  returns the number of bytes returned in the temporary space,
		  or a negative value on error.

	6) int peek(char &c)
		- gets the next character in the stream without consuming it.
		  returns 1 on success, 0 on error.

	7) struct sockaddr_in *endpoint();
		- get the sockaddr_in structure which defines the connections
		peer address/port 

	8) int get_port()
		- get the IP port of the underlying socket

	9) int timeout()
		- set a timeout value for all connects/accepts/reads/writes of socket

	10) int end_of_message()
		- on encode, flush stream and send record delimiter.  on decode, discard
		data up until the next record delimiter.

	* CODE/PUT/GET:

	- Put (and Get) routines write to (and read from) streams,
	based on the value of the encoding scheme used.
	- The Code routines call either put or get based on the
	the which of encode() or decode() was called last.
	- All return TRUE on success and FALSE on failure.


	- Suggestions:

	The suggested way to extend on the encode/decode capabilities
	for more complex objects is the following:

	- int MyObject::code(Stream &);  { calls on base code routines }
	- inline int MyObject::put(Stream &s) { s.encode(); return code(s); }
	- inline int MyObject::get(Stream &s) { s.decode(); return code(s); }

*/

#ifndef STREAM_H
#define STREAM_H

#include <assert.h>

#include "condor_common.h"

#define eom end_of_message

#include "proc.h"

/* now include sched.h.  cleanup namespace if user has not
 * already included condor_mach_status.h, otherwise leave alone.
 * this silliness is needed because other parts of the code use
 * an enumerated type which has CHECKPOINTING and SUSPENDED defined,
 * and g++ apparently handles enums via #defines behind the scene. -Todd 7/97
 */
#ifndef _MACH_STATUS
#	include "sched.h"
#	undef CHECKPOINTING
#	undef SUSPENDED
#else
#	include "sched.h"
#endif

#include "condor_constants.h"	/* to get BOOLEAN typedef... */
#include "startup.h"

#if !defined(WIN32)

#include <sys/stat.h>

#if defined(OSF1)
#  include <sys/mount.h>
#elif defined(LINUX) || defined(HPUX9)
#  include <sys/vfs.h>
#else
#  include <sys/statfs.h>
#endif

#endif // !defined(WIN32)

class Stream {

/*
**		PUBLIC INTERFACE TO ALL STREAMS
*/
public:

	/*
	**	Type definitions
	*/

	enum stream_type { file, incore, safe_sock, reli_sock };
	enum stream_code { internal, external, ascii };


	/*
	**	Methods
	*/

	//	error messages
	//
	inline int valid() const { return _error == stream_valid; }
	inline int ok() const { return _error == stream_valid; }

	//	coding type operations
	//
	inline stream_code representation() const { return _code; }
	inline void set_representation(stream_code c) { _code = c; }
	inline void encode() { _coding = stream_encode; }
	inline void decode() { _coding = stream_decode; }
	inline int is_encode() const { return _coding == stream_encode; }
	inline int is_decode() const { return _coding == stream_decode; }



	//	code/decode operations
	//

	int code(char &);
	int code(unsigned char &);
	int code(int &);
	int code(unsigned int &);
	int code(long &);
	int code(unsigned long &);
	int code(short &);
	int code(unsigned short &);
	int code(float &);
	int code(double &);
	int code(char *&);
	int code(char *&, int &);
	int code_bytes(void *, int);
	int code_bytes_bool(void *, int);

	//	Condor types

	int code(PROC_ID &);
	int code(PROC &);
	int code(STARTUP_INFO &);
	int code(PORTS &);
	int code(StartdRec &);

#if !defined(WIN32)

	//  UNIX types

	int signal(int &);

	int code(struct rusage &);
	int code(struct stat &);
	int code(struct statfs &);
	int code(struct timezone &);
	int code(struct timeval &);
	int code(struct rlimit &);

#endif // !defined(WIN32)

	//   allow pointers instead of references to ease XDR compatibility
	//

	int code(unsigned char *x)		{ return code(*x); }
	int code(int *x) 				{ return code(*x); }
	int code(unsigned int *x) 		{ return code(*x); }
	int code(long *x) 				{ return code(*x); }
	int code(unsigned long *x) 		{ return code(*x); }
	int code(short *x) 				{ return code(*x); }
	int code(unsigned short *x) 	{ return code(*x); }
	int code(float *x) 				{ return code(*x); }
	int code(double *x) 			{ return code(*x); }

#if !defined(WIN32)
	
	int signal(int *x)				{ return signal(*x); }

	int code(PROC_ID *x)			{ return code(*x); }
	int code(struct rusage *x)		{ return code(*x); }
	int code(PROC *x)				{ return code(*x); }
	int code(STARTUP_INFO *x)		{ return code(*x); }
	int code(struct stat *x)		{ return code(*x); }
	int code(struct statfs *x)		{ return code(*x); }
	int code(struct timezone *x)	{ return code(*x); }
	int code(struct timeval *x)		{ return code(*x); }
	int code(struct rlimit *x)		{ return code(*x); }
	int code(PORTS *x)				{ return code(*x); }
	int code(StartdRec *x)			{ return code(*x); }

#endif // !defined(WIN32)

	//	Put operations
	//

	int put(char);
	int put(unsigned char);
	int put(int);
	int put(unsigned int);
	int put(long);
	int put(unsigned long);
	int put(short);
	int put(unsigned short);
	int put(float);
	int put(double);
	int put(char *);
	int put(char *, int);


	//	get operations
	//

	int get(char &);
	int get(unsigned char &);
	int get(int &);
	int get(unsigned int &);
	int get(long &);
	int get(unsigned long &);
	int get(short &);
	int get(unsigned short &);
	int get(float &);
	int get(double &);
	int get(char *&);
	int get(char *&, int &);

	/*
	**	Stream protocol
	*/

	virtual ~Stream() {} 

	//	byte operations (virtually defined by each stream)
	//
	virtual int put_bytes(const void *, int) { assert(0); return 0; }
	virtual int get_bytes(void *, int) { assert(0); return 0; }
	virtual int get_ptr(void *&, char) { assert(0); return 0; }
	virtual int peek(char &) { assert(0); return 0; }
	virtual int end_of_message() { assert(0); return 0; }
//	int eom() { return end_of_message(); }

	// peer information operations (virtually defined by each stream)
	//
	virtual struct sockaddr_in *endpoint() { assert(0); return (struct sockaddr_in *)0; }

	// local port information (virtually defined by each stream)
	//
	virtual int get_port() { assert(0); return 0; }

	// set a timeout for an underlying socket
	//
	virtual int timeout(int) { assert(0); return 0; }

	//	type operation
	//
	virtual stream_type type() { assert(0); return (stream_type)0; }

	// Condor Compatibility Ops
	int snd_int(int val, int end_of_record);
	int rcv_int(int &val, int end_of_record);


/*
**		PRIVATE INTERFACE TO ALL STREAMS
*/
protected:

	/*
	**	Type definitions
	*/

	enum stream_coding { stream_decode, stream_encode };
	enum stream_error { stream_valid, stream_invalid };


	/*
	**	Methods
	*/

	//	constructor
	//
	Stream(stream_code c=external)
		: _error(stream_valid), _code(c), _coding(stream_encode)
		{}


	/*
	**	Data structures
	*/

	stream_error	_error;
	stream_code		_code;
	stream_coding	_coding;
};



#endif
