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

#include "condor_common.h"
#include "CryptKey.h"

KeyInfo:: KeyInfo()
    : keyData_    (0),
      keyDataLen_ (0),
      protocol_   (CONDOR_NO_PROTOCOL),
      duration_   (0)
{
    
}

KeyInfo :: KeyInfo(const KeyInfo& copy)
    : keyData_    ( 0 ),
      keyDataLen_ (copy.keyDataLen_),
      protocol_   (copy.protocol_),
      duration_   (copy.duration_)
{
    init(copy.keyData_, copy.keyDataLen_);
}

KeyInfo :: KeyInfo(unsigned char * keyData,
                   int             keyDataLen,
                   Protocol        protocol,
                   int             duration )
    : protocol_   (protocol),
      keyData_    (0),
      keyDataLen_ (keyDataLen),
      duration_   (duration)
{
    init(keyData, keyDataLen);
}

KeyInfo& KeyInfo :: operator=(const KeyInfo& copy)
{
	if (&copy != this) {
		if (keyData_) {
			free(keyData_);
			keyData_ = 0;
		}
		
		keyDataLen_ = copy.keyDataLen_;
		protocol_   = copy.protocol_;
		duration_   = copy.duration_;

		init(copy.keyData_, copy.keyDataLen_);
	}

    return *this;
}

void KeyInfo :: init(unsigned char * keyData, int keyDataLen)
{
    if ((keyDataLen > 0) && keyData) {
        keyDataLen_ = keyDataLen;
        keyData_    = (unsigned char *)malloc(keyDataLen_ + 1);
        memset(keyData_, 0, keyDataLen_ + 1);
        memcpy(keyData_, keyData, keyDataLen_);   
    }
    else {
        keyDataLen_ = 0;
    }
}

KeyInfo :: ~KeyInfo()
{
    if (keyData_) {
        free(keyData_);
    }
    keyData_ = 0;
}

const unsigned char * KeyInfo :: getKeyData() const
{
    return keyData_;
}
    
int KeyInfo :: getKeyLength() const
{
    return keyDataLen_;
}

Protocol KeyInfo :: getProtocol() const
{
    return protocol_;
}

int KeyInfo :: getDuration() const
{
    return duration_;
}
