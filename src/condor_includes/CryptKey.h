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


#ifndef CONDOR_CRYPT_KEY
#define CONDOR_CRYPT_KEY

#include <memory>

enum Protocol {
    CONDOR_NO_PROTOCOL,
    CONDOR_BLOWFISH,
    CONDOR_3DES,
    CONDOR_AESGCM
};

struct CryptoState {
		// The IV is split in two: a 32-bit counter and a
		// 64-bit random number for a total of 12 bytes.
		// The ctr is added to the last 4 bytes of the IV.
	union Packed_IV {
		unsigned char iv[12]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                struct {
		    uint32_t pkt;
		    uint32_t conn;
                } ctr;
	};

	uint32_t m_ctr_enc{0}; // Number of outgoing (encrypted) packets
	uint32_t m_ctr_dec{0}; // Number of incoming (decrypted) packets
	uint32_t m_ctr_conn{0}; // Number of times this session has been used by a connection
	union Packed_IV m_iv_enc; // IV for outgoing data
	union Packed_IV m_iv_dec; // IV for incoming data.
        unsigned char m_prev_mac_enc[16]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        unsigned char m_prev_mac_dec[16]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

class KeyInfo {
 public:
    KeyInfo();
    //------------------------------------------
    // Default constructor
    //------------------------------------------

    KeyInfo(const unsigned char * keyData,
            int             keyDataLen,
            Protocol        protocol,
            int             duration,
            std::shared_ptr<CryptoState> state);
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

	/** Returns a padded key.
		@param len Minimum length of padded key in bytes.
		@return A buffer with the padded key that be deallocated via free() 
				by the caller, or NULL on failure.
	*/
	unsigned char * getPaddedKeyData(int len) const;

    std::shared_ptr<CryptoState> getCryptoState() const {return state_;}

 private:
    void init(const unsigned char * keyData, int keyDataLen);

    unsigned char * keyData_;
    int             keyDataLen_;
	//int				keyBufferLen_;
    Protocol        protocol_;
    int             duration_;

    std::shared_ptr<CryptoState> state_;
};

#endif
