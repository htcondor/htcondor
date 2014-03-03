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
#include "condor_md.h"
#include "condor_open.h"
#include "condor_debug.h"

#ifdef HAVE_EXT_OPENSSL
#include <openssl/md5.h>
#endif

class MD_Context {
public:
#ifdef HAVE_EXT_OPENSSL
    MD5_CTX          md5_;     // MD5 context
#endif
};

Condor_MD_MAC::Condor_MD_MAC()
    : context_ (new MD_Context()),
      key_     (0)
{
    init();
}
    
Condor_MD_MAC::Condor_MD_MAC(KeyInfo * key)
    : context_ (new MD_Context()),
      key_     (0)
{
    key_ = new KeyInfo(*key);
    init();
}

Condor_MD_MAC :: ~Condor_MD_MAC()
{
    delete key_;
    delete context_;
}

void Condor_MD_MAC :: init()
{
#ifdef HAVE_EXT_OPENSSL
    MD5_Init(&(context_->md5_));
    
    if (key_) {
        addMD(key_->getKeyData(), key_->getKeyLength());
    }
#endif
}

unsigned char * Condor_MD_MAC::computeOnce(unsigned char * buffer, unsigned long length)
{
#ifdef HAVE_EXT_OPENSSL
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);

    return MD5(buffer, (unsigned long) length, md);
#else
    return NULL;
#endif
}

unsigned char * Condor_MD_MAC::computeOnce(unsigned char * buffer, 
                                           unsigned long   length, 
                                           KeyInfo       * key)
{
#ifdef HAVE_EXT_OPENSSL
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
    MD5_CTX  context;

    MD5_Init(&context);

    MD5_Update(&context, key->getKeyData(), key->getKeyLength());

    MD5_Update(&context, buffer, length);

    MD5_Final(md, &context);

    return md;
#else
    return NULL;
#endif
}

bool Condor_MD_MAC::verifyMD(unsigned char * md, 
                             unsigned char * buffer, 
                             unsigned long   length)
{
#ifdef HAVE_EXT_OPENSSL
    unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length);
    bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
    free(myMd);
    return match;
#else
    return true;
#endif
}

bool Condor_MD_MAC::verifyMD(unsigned char * md, 
                             unsigned char * buffer, 
                             unsigned long   length, 
                             KeyInfo       * key)
{
#ifdef HAVE_EXT_OPENSSL
   unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length, key); 
   bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
   free(myMd);
   return match;
#else
   return true;
#endif
}
  
void Condor_MD_MAC::addMD(const unsigned char * buffer, unsigned long length)
{
#ifdef HAVE_EXT_OPENSSL
   MD5_Update(&(context_->md5_), buffer, length); 
#endif
}

bool Condor_MD_MAC::addMDFile(const char * filePathName)
{
#ifdef HAVE_EXT_OPENSSL
	int fd;

    fd = safe_open_wrapper_follow(filePathName, O_RDONLY | O_LARGEFILE, 0);
    if (fd < 0) {
        dprintf(D_ALWAYS,
                "addMDFile: can't open %s: %s\n",
                filePathName,
                strerror(errno));
        return false;
    }

	unsigned char *buffer;	

	buffer = (unsigned char *)calloc(1024*1024, 1);
	ASSERT(buffer != NULL);

	bool ok = true;
	ssize_t count = read(fd, buffer, 1024*1024); 
	while( count > 0) {
		MD5_Update(&(context_->md5_), buffer, count); 
		memset(buffer, 0, 1024*1024);
		count = read(fd, buffer, 1024*1024); 
	}
	if (count == -1) {
		dprintf(D_ALWAYS,
		        "addMDFile: error reading from %s: %s\n",
		        filePathName,
		        strerror(errno));
		ok = false;
	}

	close(fd);
	free(buffer);
	return ok;
#else
	return false;
#endif
}

unsigned char * Condor_MD_MAC::computeMD()
{
#ifdef HAVE_EXT_OPENSSL
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
    
    MD5_Final(md, &(context_->md5_));

    init(); // reinitialize for the next round

    return md;
#else 
    return NULL;
#endif
}

bool Condor_MD_MAC :: verifyMD(unsigned char * md)
{
#ifdef HAVE_EXT_OPENSSL
    unsigned char * md2 = computeMD();
    bool match = (memcmp(md, md2, MAC_SIZE) == 0);
    free(md2);
    return match;
#else
    return true;
#endif
}

