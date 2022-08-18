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

#include <openssl/evp.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#  define EVP_MD_CTX_new   EVP_MD_CTX_create
#  define EVP_MD_CTX_free  EVP_MD_CTX_destroy
#endif

class MD_Context {
public:
	EVP_MD_CTX			*mdctx_ = NULL;
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
	EVP_MD_CTX_free(context_->mdctx_);
    delete key_;
    delete context_;
}

void Condor_MD_MAC :: init()
{
	//If a message digest context already exists free it and get a new one
	if (context_->mdctx_) {
		EVP_MD_CTX_free(context_->mdctx_);
		context_->mdctx_ = NULL;
	}
	context_->mdctx_ = EVP_MD_CTX_new();
#ifdef FIPS_MODE
	EVP_DigestInit_ex(context_->mdctx_, EVP_sha256(), NULL);
    if (key_) {
        addMD(key_->getKeyData(), key_->getKeyLength());
    }
#else
	EVP_DigestInit_ex(context_->mdctx_, EVP_md5(), NULL);
    if (key_) {
        addMD(key_->getKeyData(), key_->getKeyLength());
    }
#endif
}

unsigned char * Condor_MD_MAC::computeOnce(const unsigned char * buffer, unsigned long length)
{
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
#ifdef FIPS_MODE
	EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
#else
	EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
#endif
	EVP_DigestUpdate(mdctx, buffer, length);
	EVP_DigestFinal_ex(mdctx, md, NULL);
	EVP_MD_CTX_free(mdctx);
	
	return md;
}

unsigned char * Condor_MD_MAC::computeOnce(const unsigned char * buffer,
                                           unsigned long length, 
                                           KeyInfo       * key)
{
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
#ifdef FIPS_MODE
	EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
#else
	EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
#endif
	EVP_DigestUpdate(mdctx, key->getKeyData(), key->getKeyLength());
	EVP_DigestUpdate(mdctx, buffer, length);
	EVP_DigestFinal_ex(mdctx, md, NULL);
	EVP_MD_CTX_free(mdctx);

    return md;
}

bool Condor_MD_MAC::verifyMD(const unsigned char * md,
                             const unsigned char * buffer,
                             unsigned long length)
{
    unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length);
    bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
    free(myMd);
    return match;
}

bool Condor_MD_MAC::verifyMD(const unsigned char * md,
                             const unsigned char * buffer,
                             unsigned long length, 
                             KeyInfo       * key)
{
   unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length, key); 
   bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
   free(myMd);
   return match;
}
  
void Condor_MD_MAC::addMD(const unsigned char * buffer, unsigned long length)
{
	EVP_DigestUpdate(context_->mdctx_, buffer, length);
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
		EVP_DigestUpdate(context_->mdctx_, buffer, count);
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

	EVP_DigestFinal_ex(context_->mdctx_, md, NULL);
	
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

