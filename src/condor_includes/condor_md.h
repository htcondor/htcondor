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
