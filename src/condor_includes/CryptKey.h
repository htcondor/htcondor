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
