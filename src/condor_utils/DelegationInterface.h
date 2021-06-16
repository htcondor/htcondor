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

/* A class to hold an X509 credential, including it public certificate,
 * private key, and a chain of any parent non-CA certificates (for proxies).
 * The class is heavily focused on supporting delegation of proxy 
 * certificate over the network.
 */
class X509Credential
{
  EVP_PKEY* key_; /** Private key */
  X509* cert_;/** Public key/certificate corresponding to private key */
  STACK_OF(X509)* chain_; /** Chain of other certificates needed to verify 'cert_' if any */
  bool GenerateKey(void); /** Creates private key */
  void LogError(void);
  void CleanError(void);
 public:
  /** Creates object with new private key.
     Intended for starting a delegation request. */
  X509Credential(void);
  /** Creates instance from provided credentials.
     Arguments should contain PEM-encoded certificate, private key and 
     optionally certificates chain. */
  X509Credential(const std::string& credentials);
  /** Creates instance from provided credentials.
     Arguments should contain filesystem path to PEM-encoded certificate and
     private key. Optionally cert_file may contain private key and
     certificates chain. */
  X509Credential(const std::string& cert_file,const std::string& key_file,const std::string& inpwd = "");
  ~X509Credential(void);
  EVP_PKEY* GetKey() { return key_; };
  X509* GetCert() { return cert_; };
  STACK_OF(X509)* GetChain() { return chain_; };

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

  /** Perform delegation.
    Takes X509 certificate request and creates proxy credentials
   excluding private key. Result is then to be fed into 
   DelegationConsumer::Acquire */
  std::string Delegate(const std::string& request,const DelegationRestrictions& restrictions = DelegationRestrictions());
  BIO* Delegate(BIO* request,const DelegationRestrictions& restrictions = DelegationRestrictions());
  X509* Delegate(X509_REQ* request,const DelegationRestrictions& restrictions = DelegationRestrictions());
};

#endif /* __ARC_DELEGATIONINTERFACE_H__ */
