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

#include "CryptKey.h"

KeyInfo:: KeyInfo()
    : protocol_   (CONDOR_NO_PROTOCOL),
      keyData_    (0),
      keyDataLen_ (0),
      duration_   (0)
{
    
}

KeyInfo :: KeyInfo(const KeyInfo& copy)
    : protocol_   (copy.protocol_),
      keyData_    (new unsigned char[copy.keyDataLen_]),
      keyDataLen_ (copy.keyDataLen_),
      duration_   (copy.duration_)
{
    memcpy(keyData_, copy.keyData_, keyDataLen_);   
}

KeyInfo :: KeyInfo(unsigned char * keyData,
                   int             keyDataLen,
                   Protocol        protocol,
                   int             duration )
    : protocol_   (protocol),
      keyData_    (new unsigned char[keyDataLen]),
      keyDataLen_ (keyDataLen),
      duration_   (duration)
{
    memcpy(keyData_, keyData, keyDataLen);
}

KeyInfo :: ~KeyInfo()
{
    delete keyData_;
}

unsigned char * KeyInfo :: getKeyData()
{
    return keyData_;
}
    
int KeyInfo :: getKeyLength()
{
    return keyDataLen_;
}

Protocol KeyInfo :: getProtocol()
{
    return protocol_;
}

int KeyInfo :: getDuration()
{
    return duration_;
}
