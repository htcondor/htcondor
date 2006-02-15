/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 
#ifndef STREAM_H
#define STREAM_H

#include "condor_common.h"
#include "condor_crypt.h"             // For now, we include it
#include "CryptKey.h"                 // KeyInfo
// This #define is a desperate move. GNU g++ seems to choke at runtime on our
// inline function for eom.  Someday not needed ?
#define eom end_of_message

enum CONDOR_MD_MODE {
    MD_OFF        = 0,         // off
    MD_ALWAYS_ON,              // always on, condor will check MAC automatically
    MD_EXPLICIT                // user needs to call checkMAC explicitly
};

const int CLEAR_HEADER     = 0;
const int MD_IS_ON         = 1;
const int ENCRYPTION_IS_ON = 2;

const condor_mode_t NULL_FILE_PERMISSIONS = (condor_mode_t)0;

#include "proc.h"
#include "condor_old_shadow_types.h"

#include "startup.h"

/** @name Special Types
    We need to define a special code() method for certain integer arguments.
    To take advantage of overloading, we need make these arguments have a
    new type.  Since typedef doesn't actually create a new type in C++, we
    use enum.  Note that we explicitly define MAXINT as an allowable value
    for the enum to make sure sizeof(enum) == sizeof(int).
*/

//@{
	/// look in cedar_enums.h
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
	int code(void *&);
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
#else
	 // windows
	int code( LONGLONG &);
	///
	int code( ULONGLONG &);
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
    ///
	int code(condor_errno_t &);

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
	///
	int code(condor_mode_t &);

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
    ///
	int code(condor_errno_t *x)		{ return code(*x); }

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
#else
	// Windows
	int put(LONGLONG);
	int put(ULONGLONG);
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
#else
	// Windows
	int get(LONGLONG &);
	int get(ULONGLONG &);
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
	virtual ~Stream(); 

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

	///
	virtual void allow_one_empty_message();

	/// set a timeout for an underlying socket
	virtual int timeout(int) = 0;

	/// get timeout time for pending connect operation;
	virtual time_t connect_timeout_time() = 0;

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

        //------------------------------------------
        // Encryption support below
        //------------------------------------------
        bool set_crypto_key(bool enable, KeyInfo * key, const char * keyId=0);
        //------------------------------------------
        // PURPOSE: set sock to use a particular encryption key
        // REQUIRE: KeyInfo -- a wrapper for keyData
        // RETURNS: true -- success; false -- failure
        //------------------------------------------

        void set_crypto_mode(bool enable);
        //------------------------------------------
        // PURPOSE: enable or disable encryption
        // REQUIRE: bool, true -- on; false -- off
        // RETURNS:
        //------------------------------------------

        bool get_encryption() const;
        //------------------------------------------
        // PURPOSE: Return encryption mode
        // REQUIRE: None
        // RETURNS: true -- on, false -- off
        //------------------------------------------

        bool wrap(unsigned char* input, int input_len, 
                  unsigned char*& output, int& outputlen);
        //------------------------------------------
        // PURPOSE: encrypt some data
        // REQUIRE: Protocol, keydata. set_encryption_procotol
        //          must have been called and encryption_mode is on
        // RETURNS: TRUE -- success, FALSE -- failure
        //------------------------------------------

        bool unwrap(unsigned char* input, int input_len, 
                    unsigned char*& output, int& outputlen);
        //------------------------------------------
        // PURPOSE: decrypt some data
        // REQUIRE: Protocol, keydata. set_encryption_procotol
        //          must have been called and encryption_mode is on
        // RETURNS: TRUE -- success, FALSE -- failure
        //------------------------------------------

        //----------------------------------------------------------------------
        // MAC/MD related stuff
        //----------------------------------------------------------------------
        bool set_MD_mode(CONDOR_MD_MODE mode, KeyInfo * key = 0, const char * keyid = 0);    
        //virtual bool set_MD_off() = 0;
        //------------------------------------------
        // PURPOSE: set mode for MAC (on or off)
        // REQUIRE: mode -- see the enumeration defined above
        //          key  -- an optional key for the MAC. if null (by default)
        //                  all CEDAR does is send a Message Digest over
        //                  When key is specified, this is essentially a MAC
        // RETURNS: true -- success; false -- false
        //------------------------------------------

        bool isOutgoing_MD5_on() const { return (mdMode_ == MD_ALWAYS_ON); }
        //------------------------------------------
        // PURPOSE: whether MD is turned on or not
        // REQUIRE: None
        // RETURNS: true -- MD is on; 
        //          false -- MD is off
        //------------------------------------------

        virtual const char * isIncomingDataMD5ed() = 0;
        //------------------------------------------
        // PURPOSE: To check to see if incoming data
        //          has MD5 checksum/. NOTE! Currently,
        //          this method should be used with UDP only!
        // REQUIRE: None
        // RETURNS: NULL -- data does not contain MD5
        //          key id -- if the data is checksumed
        //------------------------------------------
    //@}
 private:
        bool initialize_crypto(KeyInfo * key);
        //------------------------------------------
        // PURPOSE: initialize crypto
        // REQUIRE: KeyInfo
        // RETURNS: None
        //------------------------------------------
        
/*
**		PRIVATE INTERFACE TO ALL STREAMS
*/
protected:

        virtual bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId) = 0;
        virtual bool set_encryption_id(const char * keyId) = 0;
        const KeyInfo& get_crypto_key() const;
        const KeyInfo& get_md_key() const;
      
        void resetCrypto();

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
	Stream(stream_code c=external);
	/*
	**	Data structures
	*/

        Condor_Crypt_Base * crypto_;         // The actual crypto
        bool                crypto_mode_;    // true == enabled, false == disabled.
        CONDOR_MD_MODE      mdMode_;        // MAC mode
        KeyInfo           * mdKey_;
        bool                encrypt_;        // Encryption mode
	stream_code	    _code;
	stream_coding	    _coding;

	int allow_empty_message_flag;
};



#endif
