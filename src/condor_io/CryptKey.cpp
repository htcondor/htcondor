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

KeyInfo :: KeyInfo(const unsigned char * keyData,
                   size_t          keyDataLen,
                   Protocol        protocol,
                   int             duration )
    : 
	  protocol_   (protocol),
      duration_   (duration)
{
	keyData_.resize(keyDataLen);
	memcpy(keyData_.data(), keyData, keyDataLen);
}

const unsigned char * KeyInfo :: getKeyData() const
{
    return keyData_.data();
}
    
size_t KeyInfo :: getKeyLength() const
{
    return keyData_.size();
}

Protocol KeyInfo :: getProtocol() const
{
    return protocol_;
}

int KeyInfo :: getDuration() const
{
    return duration_;
}

unsigned char * KeyInfo :: getPaddedKeyData(size_t len) const
{
	unsigned char* padded_key_buf = NULL;
	size_t i;

		// fail if we have no key to pad!
	if ( keyData_.size() < 1   ) {
		return NULL;
	}

		// Allocate new buffer that the caller must free()
	padded_key_buf = (unsigned char *)malloc(len);
	ASSERT(padded_key_buf);
	memset(padded_key_buf, 0, len);

		// If the key is larger than we want, just copy a prefix
		// and XOR the remainder back to the start of the buffer
	if(keyData_.size() > len) {
		memcpy(padded_key_buf, keyData_.data(), len);
			// XOR in the rest so two keys longer than the requested length
			// do not appear equal to the caller.
		for (i=len; i < keyData_.size(); i++) {
			padded_key_buf[ i % len ] ^= keyData_[i];
		}
		return padded_key_buf;
	}
		// copy the key into our new large-sized buffer
	memcpy(padded_key_buf, keyData_.data(), keyData_.size());

		// Pad the key by if needed by
		// simply repeating the key over and over until the 
		// desired length is obtained.
	for ( i = keyData_.size() ; i < len; i++ ) {
		padded_key_buf[i] = padded_key_buf[ i - keyData_.size() ];
	}
	
	return padded_key_buf;
}
