/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 
#ifndef STREAM_H
#define STREAM_H

#include "condor_common.h"

// This #define is a desperate move. GNU g++ seems to choke at runtime on our
// inline function for eom.  Someday not needed ?
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

#include "startup.h"

/** @name Special Types
    We need to define a special code() method for certain integer arguments.
    To take advantage of overloading, we need make these arguments have a
    new type.  Since typedef doesn't actually create a new type in C++, we
    use enum.  Note that we explicitly define MAXINT as an allowable value
    for the enum to make sure sizeof(enum) == sizeof(int).
*/

//@{

///
enum condor_signal_t { __signal_t_dummy_value = INT_MAX };
///
enum open_flags_t { __open_flags_t_dummy_value = INT_MAX };
///
enum fcntl_cmd_t { __fcntl_cmd_t_dummy_value = INT_MAX };

//@}


/** The stream library.

    <h3> STEAM COMPONENTS </h3>

    The condor stream library consists of (for now)<br>
    <ul>
	  <li> File streams (file.h)
	  <li> InCore streams (incore.h)
	  <li> Network socket streams (socks.h)
      <ul>
	    <li> Reliable network streams (on top of tcp)
	    <li> Safe network streams (on top of udp)
      </ul>
    </ul>


	<h3> DATA REPRESENTATION </h3>

	Streams can be in 3 modes:
    <ul>
	  <li> Internal binary representation
	  <li> External binary representation
	  <li> Ascii representation	(not implemented)
    </ul>

	<h3> CODING/DECODING </h3>

	In the coding/decoding environment, there are 4 base objects:
    <ul>
	  <li> char
	  <li> int/long/short
	  <li> float/double
	  <li> null terminated (or plain null) strings
    </ul>

	<h3> PROTOCOL </h3>

	The Stream base class defines the private protocol for all Streams.
	This protocol consists of 9 virtual functions:

    <pre>
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

	7) int timeout()
		- set a timeout value for all connects/accepts/reads/writes of socket

	8) int end_of_message()
		- on encode, flush stream and send record delimiter.  on decode, discard
		data up until the next record delimiter.

	9) char * serialize()
		- save/restore object state
    </pre>

	<h3> CODE/PUT/GET </h3>

    <ul>
      <li> Put (and Get) routines write to (and read from) streams,
           based on the value of the encoding scheme used.
      <li> The Code routines call either put or get based on the
           the which of encode() or decode() was called last.
      <li> All return TRUE on success and FALSE on failure.
    </ul>

	<h3> Suggestions </h3>

	The suggested way to extend on the encode/decode capabilities
	for more complex objects is the following:

    <pre>
	int MyObject::code(Stream &);  { calls on base code routines }
	inline int MyObject::put(Stream &s) { s.encode(); return code(s); }
	inline int MyObject::get(Stream &s) { s.decode(); return code(s); }
    </pre>
*/
class Stream {

public:

	friend class DaemonCore;

    ///
	enum stream_type {
        /** */ file,
        /** */ incore,
        /** */ safe_sock,
        /** */ reli_sock
    };

    ///
	enum stream_code {
        /** */ internal,
        /** */ external,
        /** */ ascii
    };

    ///
	inline stream_code representation() const { return _code; }
    ///
	inline void set_representation(stream_code c) { _code = c; }

    ///
	inline void encode() { _coding = stream_encode; }
    ///
	inline void decode() { _coding = stream_decode; }
    ///
	inline int is_encode() const { return _coding == stream_encode; }
    ///
	inline int is_decode() const { return _coding == stream_decode; }



	/** @name Code Operations
     */

	//@{

    /** @name Basic Types
     */
    //@{

    ///
	int code(char &);
    ///
	int code(unsigned char &);
    ///
	int code(int &);
    ///
	int code(unsigned int &);
    ///
	int code(long &);
    ///
	int code(unsigned long &);
#ifndef WIN32
    ///
	int code(long long &);
    ///
	int code(unsigned long long &);
#endif
    ///
	int code(short &);
    ///
	int code(unsigned short &);
    ///
	int code(float &);
    ///
	int code(double &);
    ///
	int code(char *&);
    ///
	int code(char *&, int &);
    ///
	int code_bytes(void *, int);
    ///
	int code_bytes_bool(void *, int);

    //@}

	/** @name Condor Types
     */
    //@{
    ///
	int code(PROC_ID &);
    ///
	int code(STARTUP_INFO &);
    ///
	int code(PORTS &);
    ///
	int code(StartdRec &);
    //@}

	/** @name UNIX Types
     */
    //@{

    ///
	int code(open_flags_t &);
    ///
	int code(struct stat &);
#if !defined(WIN32)
    ///
	int code(condor_signal_t &);
    ///
	int code(fcntl_cmd_t &);
    ///
	int code(struct rusage &);
    ///
	int code(struct statfs &);
    ///
	int code(struct timezone &);
    ///
	int code(struct timeval &);
    ///
	int code(struct utimbuf &);
    ///
	int code(struct rlimit &);
    ///
	int code_array(gid_t *&array, int &len);
    ///
	int code(struct utsname &);
#endif // !defined(WIN32)

#if HAS_64BIT_STRUCTS
    ///
	int code(struct stat64 &);
    ///
	int code(struct rlimit64 &);
#endif
    //@}

	/** @name Pointer Types.
        Allow pointers instead of references to ease XDR compatibility
    */
    //@{

    ///
	int code(unsigned char *x)		{ return code(*x); }
    ///
	int code(int *x) 				{ return code(*x); }
    ///
	int code(unsigned int *x) 		{ return code(*x); }
    ///
	int code(long *x) 				{ return code(*x); }
    ///
	int code(unsigned long *x) 		{ return code(*x); }
    ///
	int code(short *x) 				{ return code(*x); }
    ///
	int code(unsigned short *x) 	{ return code(*x); }
    ///
	int code(float *x) 				{ return code(*x); }
    ///
	int code(double *x) 			{ return code(*x); }
    ///
	int code(PROC_ID *x)			{ return code(*x); }
    ///
	int code(STARTUP_INFO *x)		{ return code(*x); }
    ///
	int code(PORTS *x)				{ return code(*x); }
    ///
	int code(StartdRec *x)			{ return code(*x); }

    ///
	int code(struct stat *x)		{ return code(*x); }
    ///
	int code(open_flags_t *x)		{ return code(*x); }
#if !defined(WIN32)
    ///
	int code(condor_signal_t *x)			{ return code(*x); }
    ///
	int code(fcntl_cmd_t *x) 		{ return code(*x); }
    ///
	int code(struct rusage *x)		{ return code(*x); }
    ///
	int code(struct statfs *x)		{ return code(*x); }
    ///
	int code(struct timezone *x)	{ return code(*x); }
    ///
	int code(struct timeval *x)		{ return code(*x); }
    ///
	int code(struct utimbuf *x)		{ return code(*x); }
    ///
	int code(struct rlimit *x)		{ return code(*x); }
	int code(struct utsname *x)		{ return code(*x); }
#endif // !defined(WIN32)

#if HAS_64BIT_STRUCTS
    ///
	int code(struct stat64 *x)		{ return code(*x); }
    ///
	int code(struct rlimit64 *x)	{ return code(*x); }
#endif
    //@}

    //@}

	//	Put operations
	//

	int put(char);
	int put(unsigned char);
	int put(int);
	int put(unsigned int);
	int put(long);
	int put(unsigned long);
#ifndef WIN32
	int put(long long);
	int put(unsigned long long);
#endif
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
#ifndef WIN32
	int get(long long &);
	int get(unsigned long long &);
#endif
	int get(short &);
	int get(unsigned short &);
	int get(float &);
	int get(double &);
	int get(char *&);
	int get(char *&, int &);

	/*
	**	Stream protocol
	*/

    ///
	virtual ~Stream() {} 

	/** @name Byte Operations. Virtually defined by each stream
     */
	//@{

    /** Puts bytes onto the stream
        @param data The buffer containing the bytes to put
        @param n The number of bytes to put
        @return the number actually put, or a negative value on errors
    */
	virtual int put_bytes(const void *data, int n) = 0;

    /** Get bytes from the stream
        @param data The buffer where bytes should be put
        @param maxn maximum number of bytes to get from stream
        @return the actual number of bytes read, or negative value on errors
    */
	virtual int get_bytes(void *data, int maxn) = 0;

    //@}

    /** Gets a temporary (not valid on next stream call) string with
        specified delimiter
        @param d the delimiter
        @param ptr The buffer storing the string
        @return the number of bytes returned in the temporary space, or
                a negative value on error.
    */
	virtual int get_ptr(void *& ptr, char d) = 0;

    /** Gets the next character in the stream without consuming it.
        @param c the character to get
        @return 1 on success, 0 on error
    */
	virtual int peek(char &c) = 0;

    /** Two behaviors depending on (en/de)code mode.
        On encode, flush stream and send record delimiter.
        On decode, discard data up until the next record delimiter.
        @return TRUE or FALSE
    */
	virtual int end_of_message() = 0;
//	int eom() { return end_of_message(); }

	/// set a timeout for an underlying socket
	virtual int timeout(int) = 0;

	/** Get this stream's type.
        @return the type of this stream
    */
	virtual stream_type type() = 0;

	/** @name Condor Compatibility Ops
     */
    //@{
	///
    int snd_int(int val, int end_of_record);
    ///
	int rcv_int(int &val, int end_of_record);
    //@}

/*
**		PRIVATE INTERFACE TO ALL STREAMS
*/
protected:

	// serialize object (save/restore object state to an ascii string)
	//
	virtual char * serialize(char *) = 0;
	virtual char * serialize() const = 0;

	/*
	**	Type definitions
	*/

	enum stream_coding { stream_decode, stream_encode, stream_unknown };


	/*
	**	Methods
	*/

	//	constructor
	//
	Stream(stream_code c=external)
		: _code(c), _coding(stream_encode)
		{}


	/*
	**	Data structures
	*/

	stream_code		_code;
	stream_coding	_coding;
};



#endif
