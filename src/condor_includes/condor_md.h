/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

    bool             isMAC_;       // is a MAC or a MD
    MD_Context *     context_;
    KeyInfo      *   key_;
};

#endif
