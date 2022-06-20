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

#ifdef FIPS_MODE
#include <openssl/sha.h>
#else
#include <openssl/md5.h>
#endif

class MD_Context {
public:
#ifdef FIPS_MODE
    SHA256_CTX          sha256_;     // SHA256 context
#else
    MD5_CTX             md5_;        // MD5 context
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
#ifdef FIPS_MODE
    SHA256_Init(&(context_->sha256_));

    if (key_) {
        addMD(key_->getKeyData(), key_->getKeyLength());
    }
#else
    MD5_Init(&(context_->md5_));

    if (key_) {
        addMD(key_->getKeyData(), key_->getKeyLength());
    }
#endif
}

unsigned char * Condor_MD_MAC::computeOnce(const unsigned char * buffer, unsigned long length)
{
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);

#ifdef FIPS_MODE
    return SHA256(buffer, (unsigned long) length, md);
#else
    return MD5(buffer, (unsigned long) length, md);
#endif
}

unsigned char * Condor_MD_MAC::computeOnce(const unsigned char * buffer,
                                           unsigned long   length, 
                                           KeyInfo       * key)
{
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
#ifdef FIPS_MODE
    SHA256_CTX  context;
    SHA256_Init(&context);
    SHA256_Update(&context, key->getKeyData(), key->getKeyLength());
    SHA256_Update(&context, buffer, length);
    SHA256_Final(md, &context);
#else
    MD5_CTX  context;
    MD5_Init(&context);
    MD5_Update(&context, key->getKeyData(), key->getKeyLength());
    MD5_Update(&context, buffer, length);
    MD5_Final(md, &context);
#endif

    return md;
}

bool Condor_MD_MAC::verifyMD(const unsigned char * md,
                             const unsigned char * buffer,
                             unsigned long   length)
{
    unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length);
    bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
    free(myMd);
    return match;
}

bool Condor_MD_MAC::verifyMD(const unsigned char * md,
                             const unsigned char * buffer,
                             unsigned long   length, 
                             KeyInfo       * key)
{
   unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length, key); 
   bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
   free(myMd);
   return match;
}
  
void Condor_MD_MAC::addMD(const unsigned char * buffer, unsigned long length)
{
#ifdef FIPS_MODE
   SHA256_Update(&(context_->sha256_), buffer, length);
#else
   MD5_Update(&(context_->md5_), buffer, length);
#endif
}

bool Condor_MD_MAC::addMDFile(const char * filePathName)
{
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
#ifdef FIPS_MODE
		SHA256_Update(&(context_->sha256_), buffer, count);
#else
		MD5_Update(&(context_->md5_), buffer, count);
#endif
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
}

unsigned char * Condor_MD_MAC::computeMD()
{
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
    
#ifdef FIPS_MODE
    SHA256_Final(md, &(context_->sha256_));
#else
    MD5_Final(md, &(context_->md5_));
#endif

    init(); // reinitialize for the next round

    return md;
}

bool Condor_MD_MAC :: verifyMD(const unsigned char * md)
{
    unsigned char * md2 = computeMD();
    bool match = (memcmp(md, md2, MAC_SIZE) == 0);
    free(md2);
    return match;
}

