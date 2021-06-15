/***************************************************************
 *
 * Copyright (C) Members of the NorduGrid collaboration
 *    (http://www.nordugrid.org)
 * Copyright (C) 2021, Condor Team, Computer Sciences Department,
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

/* This file was taken from the Advanced Resource Connector (ARC)
 * software codebase (https://source.coderefinery.org/nordugrid/arc.git),
 * with changes made for it to work within the HTCondor codebase.
 */

#ifndef __ARC_DELEGATIONINTERFACE_H__
#define __ARC_DELEGATIONINTERFACE_H__

#include "condor_common.h"

#include <string>
#include <list>
#include <map>
#include <openssl/bio.h>
#include <openssl/x509.h>

typedef std::map<std::string,std::string> DelegationRestrictions;

bool string_to_x509(const std::string& cert_file,const std::string& key_file,const std::string& inpwd,X509* &cert,EVP_PKEY* &pkey,STACK_OF(X509)* &cert_sk);

/** A consumer of delegated X509 credentials.
  During delegation procedure this class acquires
 delegated credentials aka proxy - certificate, private key and
 chain of previous certificates. 
  Delegation procedure consists of calling Request() method
 for generating certificate request followed by call to Acquire()
 method for making complete credentials from certificate chain. */
class DelegationConsumer {
 protected:
  void* key_; /** Private key */
  X509* cert_; /** Public key/certificate corresponding to public key */
  STACK_OF(X509)* chain_; /** Chain of other certificates needed to verify 'cert_' if any */
  bool Generate(void); /** Creates private key */
  void LogError(void);
 public:
  /** Creates object with new private key */
  DelegationConsumer(void);
  /** Creates object with provided private key */
  DelegationConsumer(const std::string& content);
  ~DelegationConsumer(void);
  operator bool(void) { return key_ != NULL; };
  bool operator!(void) { return key_ == NULL; };
  /** Return identifier of this object - not implemented */
  const std::string& ID(void);
  /** Stores content of this object into a string */
  bool Backup(std::string& content);
  /** Restores content of object from string */
  bool Restore(const std::string& content);
  /** Make X509 certificate request from internal private key */
  bool Request(std::string& content);
  bool Request(BIO* content);
  X509_REQ* Request();
  /** Ads private key into certificates chain in 'content'
     On exit content contains complete delegated credentials.  */
  bool Acquire(std::string& content);
  /** Includes the functionality of Acquire(content) plus extracting the 
     credential identity. */  
  bool Acquire(std::string& content,std::string& identity);
  bool Acquire(BIO* in, std::string& content,std::string& identity);
  bool GetInfo(std::string& content,std::string& identity);
};

/** A provider of delegated credentials.
  During delegation procedure this class generates new credential
 to be used in proxy/delegated credential. */
class DelegationProvider {
  void* key_; /** Private key used to sign delegated certificate */
  void* cert_; /** Public key/certificate corresponding to public key */
  void* chain_; /** Chain of other certificates needed to verify 'cert_' if any */
  void LogError(void);
  void CleanError(void);
 public:
  /** Creates instance from provided credentials.
     Credentials are used to sign delegated credentials.
     Arguments should contain PEM-encoded certificate, private key and 
     optionally certificates chain. */
  DelegationProvider(const std::string& credentials);
  /** Creates instance from provided credentials.
     Credentials are used to sign delegated credentials.
     Arguments should contain filesystem path to PEM-encoded certificate and
     private key. Optionally cert_file may contain certificates chain. */
  DelegationProvider(const std::string& cert_file,const std::string& key_file,const std::string& inpwd = "");
  ~DelegationProvider(void);
  operator bool(void) { return key_ != NULL; };
  bool operator!(void) { return key_ == NULL; };
  X509* GetSrcCert() { return (X509*)cert_; };
  STACK_OF(X509)* GetSrcChain() { return (STACK_OF(X509)*)chain_; };
  /** Perform delegation.
    Takes X509 certificate request and creates proxy credentials
   excluding private key. Result is then to be fed into 
   DelegationConsumer::Acquire */
  std::string Delegate(const std::string& request,const DelegationRestrictions& restrictions = DelegationRestrictions());
  BIO* Delegate(BIO* request,const DelegationRestrictions& restrictions = DelegationRestrictions());
  X509* Delegate(X509_REQ* request,const DelegationRestrictions& restrictions = DelegationRestrictions());
};

#endif /* __ARC_DELEGATIONINTERFACE_H__ */
