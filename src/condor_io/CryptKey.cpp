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


#include "condor_common.h"
#include "CryptKey.h"
#include "condor_debug.h"
#include "condor_base64.h"
#include "classad/classad.h"

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

KeyInfo::KeyInfo(classad::ClassAd const &ad) :
	keyData_(NULL),
	keyDataLen_(0),
	protocol_(CONDOR_NO_PROTOCOL),
	duration_(0)
{
	std::string encoded;
	if (ad.EvaluateAttrString("KeyInfoData", encoded))
	{
		condor_base64_decode(encoded.c_str(), &keyData_, &keyDataLen_);
	}
	int tmp;
	if (ad.EvaluateAttrInt("KeyInfoDuration", tmp))
	{
		duration_ = tmp;
	}
	if (ad.EvaluateAttrInt("KeyInfoProtocol", tmp))
	{
		protocol_ = static_cast<Protocol>(tmp);
	}
}


bool
KeyInfo::hasSerializedKey(classad::ClassAd const &ad)
{
        return ad.Lookup("KeyInfoProtocol");
}


bool
KeyInfo::serialize(classad::ClassAd &ad) const
{
	// TODO: Check error codes.
	if ((keyDataLen_ > 0) && keyData_)
	{
		char *encoded = condor_base64_encode(keyData_, keyDataLen_);
		// NOTE: condor_base64_decode only works if there's a newline in the string.
		std::string encoded2 = encoded;
		ad.InsertAttr("KeyInfoData", encoded2 + "\n");
		free(encoded); encoded=NULL;
	}
	ad.InsertAttr("KeyInfoDuration", duration_);
	ad.InsertAttr("KeyInfoProtocol", protocol_);
	return true;
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

unsigned char * KeyInfo :: getPaddedKeyData(int len) const
{
	unsigned char* padded_key_buf = NULL;
	int i;

		// fail if we have no key to pad!
	if ( keyDataLen_ < 1  || !keyData_ ) {
		return NULL;
	}

		// Allocate new buffer that the caller must free()
	padded_key_buf = (unsigned char *)malloc(len + 1);
	ASSERT(padded_key_buf);
	memset(padded_key_buf, 0, len + 1);

		// If the key is larger than we want, just copy a prefix
		// and XOR the remainder back to the start of the buffer
	if(keyDataLen_ > len) {
		memcpy(padded_key_buf, keyData_, len);
			// XOR in the rest so two keys longer than the requested length
			// do not appear equal to the caller.
		for (i=len; i < keyDataLen_; i++) {
			padded_key_buf[ i % len ] ^= keyData_[i];
		}
		return padded_key_buf;
	}
		// copy the key into our new large-sized buffer
	memcpy(padded_key_buf, keyData_, keyDataLen_);

		// Pad the key by if needed by
		// simply repeating the key over and over until the 
		// desired length is obtained.
	for ( i = keyDataLen_ ; i < len; i++ ) {
		padded_key_buf[i] = padded_key_buf[ i - keyDataLen_ ];
	}
	
	return padded_key_buf;
}
