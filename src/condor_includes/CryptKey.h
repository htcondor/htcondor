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

#ifndef CONDOR_CRYPT_KEY
#define CONDOR_CRYPT_KEY

enum Protocol {
    CONDOR_NO_PROTOCOL,
    CONDOR_BLOWFISH,
    CONDOR_3DES
};

class KeyInfo {
 public:
    KeyInfo();
    //------------------------------------------
    // Default constructor
    //------------------------------------------

    KeyInfo(unsigned char * keyData,
            int             keyDataLen,
            Protocol        protocol = CONDOR_NO_PROTOCOL,
            int             duration = 0);
    //------------------------------------------
    // Construct a key object
    //------------------------------------------

    KeyInfo(const KeyInfo& copy);
    //------------------------------------------
    // Copy constructor
    //------------------------------------------

    KeyInfo& operator=(const KeyInfo& copy);

    ~KeyInfo();

    const unsigned char * getKeyData() const;
    //------------------------------------------
    // PURPOSE: Return the key
    // REQUIRE: None
    // RETURNS: unsigned char * 
    //------------------------------------------
    
    int getKeyLength() const;
    //------------------------------------------
    // PURPOSE: Return length of the key
    // REQUIRE: None
    // RETURNS: length
    //------------------------------------------

    Protocol getProtocol() const;
    //------------------------------------------
    // PURPOSE: Return protocol
    // REQUIRE: None
    // RETURNS: protocol
    //------------------------------------------

    int getDuration() const;
    //------------------------------------------
    // PURPOSE: Return duration
    // REQUIRE: None
    // REQUIRE: None
    //------------------------------------------
 private:
    void init(unsigned char * keyData, int keyDataLen);

    unsigned char * keyData_;
    int             keyDataLen_;
    Protocol        protocol_;
    int             duration_;
};

#endif
