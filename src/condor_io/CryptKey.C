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
    : 
      keyData_    (0),
      keyDataLen_ (keyDataLen),
	  protocol_   (protocol),
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
