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


#ifndef CONDOR_MESSAGE_DIGEST_MAC
#define CONDOR_MESSAGE_DIGEST_MAC

static const int MAC_SIZE = 16;

#include "CryptKey.h"
//----------------------------------------------------------------------
// This class provides services for message digest (MD) and/or
// message authentication code (MAC) services. The code uses
// MD5 as the basis to provide these services.
//----------------------------------------------------------------------
class MD_Context;

class Condor_MD_MAC {
 public:
    Condor_MD_MAC();
    //------------------------------------------
    // PURPOSE: Create a MD object. NOT a MAC!
    //          This provides the most basic MD service
    //          it does not provide MAC service!
    // REQUIRE: None
    // RETURNS: object *this
    //------------------------------------------

    ~Condor_MD_MAC(); // destructor

    Condor_MD_MAC(KeyInfo * key);
    //------------------------------------------
    // PURPOSE: Crate a MAC object. NOT a MD!
    //          This provides a MAC service object
    // REQUIRE: NONE
    // RETURNS: object *this
    //------------------------------------------

    static unsigned char * computeOnce(unsigned char * buffer, unsigned long length);
    static unsigned char * computeOnce(unsigned char * buffer, unsigned long length, KeyInfo * key);
    static bool verifyMD(unsigned char * md, unsigned char * buffer, unsigned long length);
    static bool verifyMD(unsigned char * md, unsigned char * buffer, unsigned long length, KeyInfo * key);
    //------------------------------------------
    // PURPOSE: Compute the MAC/MD in one step
    // REQUIRE: the buffer to be checksumed
    // RETURNS: CheckSUM (MAC or MD) depends on how the object is initialized
    //------------------------------------------
    
    void addMD(const unsigned char * buffer, unsigned long length);

	// Add the contents of a file to the MD5 checksum. It handles opening
	// the file, reading it in (in chunks of 1 meg), and adding it to the
	// md5 object. returns true on success, false if an error occurs

    bool addMDFile(const char * path);
    //------------------------------------------
    // PURPOSE: If you want to compute the MAC/MD over
    //          multiple steps, use this method
    // REQUIRE: the buffer to be check sumed
    // RETURNS: none
    //------------------------------------------

    unsigned char * computeMD();
    //------------------------------------------
    // PURPOSE: Once you finished adding all the
    //          buffers by using addMultiple call
    //          A call to this method will return
    //          the checksum to you (and invalidate all internal states!)
    // REQUIRE: none
    // RETURNS: CheckSUM
    //------------------------------------------

    bool verifyMD(unsigned char * md);
    //------------------------------------------
    // PURPOSE: Once you finished adding all the
    //          buffers by using addMultiple call
    //          A call to this method will verify
    //          the checksum for you (and invalidate all internal states!)
    //          Only one of the two methods verifyMultiple, computeMultiple
    //          can be called at one time!
    // REQUIRE: none
    // RETURNS: true -- match; false -- not match
    //------------------------------------------

 private:
    
    void init();       // initialize/reinitialize MD5

    MD_Context *     context_;
    KeyInfo      *   key_;
};

#endif
