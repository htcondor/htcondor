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

#include "condor_common.h"
#include "condor_debug.h"

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>

#include <string>
#include <fstream>

#include "DelegationInterface.h"

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)

#define X509_getm_notAfter X509_get_notAfter
#define X509_getm_notBefore X509_get_notBefore
#define X509_set1_notAfter X509_set_notAfter
#define X509_set1_notBefore X509_set_notBefore

#endif

#define GLOBUS_LIMITED_PROXY_OID "1.3.6.1.4.1.3536.1.1.1.9"

//#define SERIAL_RAND_BITS 64
#define SERIAL_RAND_BITS 31

static int rand_serial(ASN1_INTEGER *ai) {
  int ret = 0;
  BIGNUM *btmp = BN_new();
  if(!btmp) goto error;
  if(!BN_pseudo_rand(btmp, SERIAL_RAND_BITS, 0, 0)) goto error;
  if(ai && !BN_to_ASN1_INTEGER(btmp, ai)) goto error;
  ret = 1;
error:
  if(btmp) BN_free(btmp);
  return ret;
}

static bool x509_to_string(X509* cert,std::string& str) {
  BIO *out = BIO_new(BIO_s_mem());
  if(!out) return false;
  if(!PEM_write_bio_X509(out,cert)) { BIO_free_all(out); return false; };
  for(;;) {
    char s[256];
    int l = BIO_read(out,s,sizeof(s));
    if(l <= 0) break;
    str.append(s,l);;
  };
  BIO_free_all(out);
  return true;
}

static bool x509_to_string(EVP_PKEY* key,std::string& str) {
  BIO *out = BIO_new(BIO_s_mem());
  if(!out) return false;
  EVP_CIPHER *enc = NULL;
  if(!PEM_write_bio_PrivateKey(out,key,enc,NULL,0,NULL,NULL)) { BIO_free_all(out); return false; };
  for(;;) {
    char s[256];
    int l = BIO_read(out,s,sizeof(s));
    if(l <= 0) break;
    str.append(s,l);;
  };
  BIO_free_all(out);
  return true;
}

static bool string_to_x509(const std::string& str,X509* &cert,EVP_PKEY* &pkey,STACK_OF(X509)* &cert_sk) {
  BIO *in = NULL;
  cert=NULL; pkey=NULL; cert_sk=NULL;
  if(str.empty()) return false;
  if(!(in=BIO_new_mem_buf((void*)(str.c_str()),str.length()))) return false;
  if((!PEM_read_bio_X509(in,&cert,NULL,NULL)) || (!cert)) { BIO_free_all(in); return false; };
  if((!PEM_read_bio_PrivateKey(in,&pkey,NULL,NULL)) || (!pkey)) { BIO_free_all(in); return false; };
  if(!(cert_sk=sk_X509_new_null())) { BIO_free_all(in); return false; };
  for(;;) {
    X509* c = NULL;
    if((!PEM_read_bio_X509(in,&c,NULL,NULL)) || (!c)) break;
    sk_X509_push(cert_sk,c);
  };
  BIO_free_all(in);
  return true;
}

static bool string_to_x509(const std::string& cert_file,const std::string& key_file,const std::string& inpwd,X509* &cert,EVP_PKEY* &pkey,STACK_OF(X509)* &cert_sk) {
  BIO *in = NULL;
  cert=NULL; pkey=NULL; cert_sk=NULL;
  if(cert_file.empty()) return false;
  if(!(in=BIO_new_file(cert_file.c_str(),"r"))) return false;
  if((!PEM_read_bio_X509(in,&cert,NULL,NULL)) || (!cert)) { BIO_free_all(in); return false; };
  if(key_file.empty()) {
    if((!PEM_read_bio_PrivateKey(in,&pkey,NULL,(void*)inpwd.c_str())) || (!pkey)) { BIO_free_all(in); return false; };
  };
  if(!(cert_sk=sk_X509_new_null())) { BIO_free_all(in); return false; };
  for(;;) {
    X509* c = NULL;
    if((!PEM_read_bio_X509(in,&c,NULL,NULL)) || (!c)) break;
    sk_X509_push(cert_sk,c);
  };
  ERR_get_error();
  if(!pkey) {
    BIO_free_all(in); in=NULL;
    if(!(in=BIO_new_file(key_file.c_str(),"r"))) return false;
    if((!PEM_read_bio_PrivateKey(in,&pkey,NULL,(void*)inpwd.c_str())) || (!pkey)) { BIO_free_all(in); return false; };
  };
  BIO_free_all(in);
  return true;
}

static bool string_to_x509(const std::string& str,X509* &cert,STACK_OF(X509)* &cert_sk) {
  BIO *in = NULL;
  if(str.empty()) return false;
  if(!(in=BIO_new_mem_buf((void*)(str.c_str()),str.length()))) return false;
  if((!PEM_read_bio_X509(in,&cert,NULL,NULL)) || (!cert)) { BIO_free_all(in); return false; };
  if(!(cert_sk=sk_X509_new_null())) { BIO_free_all(in); return false; };
  for(;;) {
    X509* c = NULL;
    if((!PEM_read_bio_X509(in,&c,NULL,NULL)) || (!c)) break;
    sk_X509_push(cert_sk,c);
  };
  ERR_get_error();
  BIO_free_all(in);
  return true;
}

static bool X509_add_ext_by_nid(X509 *cert,int nid,char *value,int pos) {
  X509_EXTENSION* ext = X509V3_EXT_conf_nid(NULL, NULL, nid, value);
  if(!ext) return false;
  X509_add_ext(cert,ext,pos);
  X509_EXTENSION_free(ext);
  return true;
}

static std::string::size_type find_line(const std::string& val, const char* token, std::string::size_type p = std::string::npos) {
  std::string::size_type l = ::strlen(token);
  if(p == std::string::npos) {
    p = val.find(token);
  } else {
    p = val.find(token,p);
  };
  if(p == std::string::npos) return p;
  if((p > 0) && (val[p-1] != '\r') && (val[p-1] != '\n')) return std::string::npos;
  if(((p+l) < val.length()) && (val[p+l] != '\r') && (val[p+l] != '\n')) return std::string::npos;
  return p;
}

static bool strip_PEM(std::string& val, const char* ts, const char* te) {
  std::string::size_type ps = find_line(val,ts);
  if(ps == std::string::npos) return false;
  ps += ::strlen(ts);
  ps = val.find_first_not_of("\r\n",ps);
  if(ps == std::string::npos) return false;
  std::string::size_type pe = find_line(val,te,ps);
  if(pe == std::string::npos) return false;
  if(pe == 0) return false;
  pe = val.find_last_not_of("\r\n",pe-1);
  if(pe == std::string::npos) return false;
  if(pe < ps) return false;
  val = val.substr(ps,pe-ps+1);
  return true;
}

static const char kBlankChars[] = " \t\n\r";
static std::string trim(const std::string& str, const char *sep) {
  if (sep == NULL)
    sep = kBlankChars;
  std::string::size_type const first = str.find_first_not_of(sep);
  return (first == std::string::npos) ? std::string() : str.substr(first, str.find_last_not_of(sep) - first + 1);
}

static void wrap_PEM(std::string& val, const char* ts, const char* te) {
  val = std::string(ts)+"\n"+trim(val,"\r\n")+"\n"+te;
}

static bool strip_PEM_request(std::string& val) {
  const char* ts = "-----BEGIN CERTIFICATE REQUEST-----";
  const char* te = "-----END CERTIFICATE REQUEST-----";
  return strip_PEM(val, ts, te);
}

#if 0
static bool strip_PEM_cert(std::string& val) {
  const char* ts = "-----BEGIN CERTIFICATE-----";
  const char* te = "-----END CERTIFICATE-----";
  return strip_PEM(val, ts, te);
}
#endif

static void wrap_PEM_request(std::string& val) {
  const char* ts = "-----BEGIN CERTIFICATE REQUEST-----";
  const char* te = "-----END CERTIFICATE REQUEST-----";
  wrap_PEM(val, ts, te);
}

#if 0
static void wrap_PEM_cert(std::string& val) {
  const char* ts = "-----BEGIN CERTIFICATE-----";
  const char* te = "-----END CERTIFICATE-----";
  wrap_PEM(val, ts, te);
}
#endif

X509Credential::X509Credential(void):key_(NULL),cert_(NULL),chain_(NULL) {
  GenerateKey();
}

X509Credential::~X509Credential(void) {
  if(key_) EVP_PKEY_free(key_);
  if(cert_) X509_free(cert_);
  if(chain_) sk_X509_pop_free(chain_, X509_free);
}

static int ssl_err_cb(const char *str, size_t len, void *u) {
  std::string& ssl_err = *((std::string*)u);
  ssl_err.append(str,len);
  return 1;
}

void X509Credential::LogError(void) {
  std::string ssl_err;
  ERR_print_errors_cb(&ssl_err_cb,&ssl_err);
  dprintf(D_ALWAYS, "Delegation error: %s\n", ssl_err.c_str());
}

void X509Credential::CleanError(void) {
  std::string ssl_err;
  ERR_print_errors_cb(&ssl_err_cb,&ssl_err);
}

bool X509Credential::GenerateKey(void) {
  bool res = false;
  int num = 2048;
  //BN_GENCB cb;
  BIGNUM *bn = BN_new();
  RSA *rsa = RSA_new();
  EVP_PKEY* pkey = EVP_PKEY_new();

  //BN_GENCB_set(&cb,&progress_cb,NULL);
  if(bn && rsa) {
    if(BN_set_word(bn,RSA_F4)) {
      //if(RSA_generate_key_ex(rsa,num,bn,&cb)) {
      if(RSA_generate_key_ex(rsa,num,bn,NULL)) {
        if(EVP_PKEY_assign_RSA(pkey, rsa)) {
          if(key_) EVP_PKEY_free(key_);
          key_=pkey; pkey=NULL; rsa=NULL; res=true;
        } else {
          LogError();
          dprintf(D_ALWAYS, "EVP_PKEY_assign_RSA failed\n");
        }
      } else {
        LogError();
        dprintf(D_ALWAYS, "RSA_generate_key_ex failed\n");
      };
    } else {
      LogError();
      dprintf(D_ALWAYS, "BN_set_word failed\n");
    };
  } else {
    LogError();
    dprintf(D_ALWAYS, "BN_new || RSA_new failed\n");
  };
  if(bn) BN_free(bn);
  if(rsa) RSA_free(rsa);
  if(pkey) EVP_PKEY_free(pkey);
  return res;
}

bool X509Credential::Request(std::string& content) {
  bool res = false;
  content.resize(0);
  X509_REQ *req = Request();
  if(req) {
    BIO *out = BIO_new(BIO_s_mem());
    if(out) {
      if(PEM_write_bio_X509_REQ(out,req)) {
        res=true;
        for(;;) {
          char s[256];
          int l = BIO_read(out,s,sizeof(s));
          if(l <= 0) break;
          content.append(s,l);;
        };
      } else {
        LogError();
        dprintf(D_ALWAYS, "PEM_write_bio_X509_REQ failed\n");
      };
      BIO_free_all(out);
    };
    X509_REQ_free(req);
  };
  return res;
}

bool X509Credential::Request(BIO* content) {
  bool res = false;
  X509_REQ *req = Request();
  if(req) {
    if(i2d_X509_REQ_bio(content,req)) {
      res=true;
    } else {
      LogError();
      dprintf(D_ALWAYS, "PEM_write_bio_X509_REQ failed\n");
    };
    X509_REQ_free(req);
  };
  return res;
}

X509_REQ* X509Credential::Request() {
  bool res = false;
  if(!key_ && !GenerateKey()) {
    return NULL;
  }
  X509_REQ *req = NULL;
  const EVP_MD *digest = EVP_sha256();
  req = X509_REQ_new();
  if(req) {
    //if(X509_REQ_set_version(req,0L)) {
    if(X509_REQ_set_version(req,2L)) {
      if(X509_REQ_set_pubkey(req,key_)) {
        if(X509_REQ_sign(req,key_,digest)) {
          res = true;
        };
      };
    };
    if(!res && req) {
      X509_REQ_free(req);
      req = NULL;
    }
  };
  return req;
}

bool X509Credential::Acquire(std::string& content) {
  std::string identity;
  return Acquire(content,identity);
}

bool X509Credential::Acquire(std::string& content, std::string& identity) {
  bool res = false;

  if(!key_) return false;
  if(cert_) return false;

  if(!string_to_x509(content, cert_, chain_)) goto err;

  if(!GetInfo(content, identity)) goto err;

  res=true;
err:
  if(!res) LogError();
  if(!res && cert_) {
    X509_free(cert_);
    cert_ = NULL;
  }
  if(!res && chain_) {
    sk_X509_pop_free(chain_, X509_free);
    chain_ = NULL;
  };
  return res;
}

bool X509Credential::Acquire(BIO* in, std::string& content, std::string& identity) {
  bool res = false;

  if(!key_) return false;
  if(cert_) return false;

  if(!(chain_=sk_X509_new_null())) goto err;

  if(!d2i_X509_bio(in, &cert_)) goto err;
  while(!BIO_eof(in))
  {
    X509* tmp_cert = NULL;
    tmp_cert = d2i_X509_bio(in, &tmp_cert);
    if(tmp_cert == NULL) goto err;
    sk_X509_push(chain_, tmp_cert);
  }

  if(!GetInfo(content, identity)) goto err;

  res=true;
err:
  if(!res) LogError();
  if(!res && cert_) {
    X509_free(cert_);
    cert_ = NULL;
  }
  if(!res && chain_) {
    sk_X509_pop_free(chain_, X509_free);
    chain_ = NULL;
  };
  return res;
}

bool X509Credential::GetInfo(std::string& content, std::string& identity) {
  bool res = false;
  std::string subject;

  if(!key_) return false;
  if(!cert_) return false;

  content.resize(0);
  if(!x509_to_string(cert_,content)) goto err;
  {
    char* buf = X509_NAME_oneline(X509_get_subject_name(cert_),NULL,0);
    if(buf) {
      subject=buf;
      OPENSSL_free(buf);
    };
  };
  if(X509_get_ext_by_NID(cert_,NID_proxyCertInfo,-1) < 0) {
    identity=subject;
  };

  if(!x509_to_string(key_,content)) goto err;
  if(chain_) {
    for(int n=0;n<sk_X509_num((STACK_OF(X509) *)chain_);++n) {
      X509* v = sk_X509_value((STACK_OF(X509) *)chain_,n);
      if(!v) goto err;
      if(!x509_to_string(v,content)) goto err;
      if(identity.empty()) {
        if(X509_get_ext_by_NID(v,NID_proxyCertInfo,-1) < 0) {
          char* buf = X509_NAME_oneline(X509_get_subject_name(v),NULL,0);
          if(buf) {
            identity=buf;
            OPENSSL_free(buf);
          };
        };
      };
    };
  };
  if(identity.empty()) identity = subject;

  res=true;
err:
  if(!res) LogError();
  return res;
}

// ---------------------------------------------------------------------------------

X509Credential::X509Credential(const std::string& credentials):key_(NULL),cert_(NULL),chain_(NULL) {
  EVP_PKEY *pkey = NULL;
  X509 *cert = NULL;
  STACK_OF(X509) *cert_sk = NULL;
  bool res = false;

//  OpenSSLInit();
  EVP_add_digest(EVP_sha256());

  if(!string_to_x509(credentials,cert,pkey,cert_sk)) goto err;
  cert_=cert; cert=NULL;
  key_=pkey; pkey=NULL;
  chain_=cert_sk; cert_sk=NULL;
  res=true;
err:
  if(!res) LogError();
  if(pkey) EVP_PKEY_free(pkey);
  if(cert) X509_free(cert);
  if(cert_sk) {
    for(int i = 0;i<sk_X509_num(cert_sk);++i) {
      X509* v = sk_X509_value(cert_sk,i);
      if(v) X509_free(v);
    };
    sk_X509_free(cert_sk);
  };
}

X509Credential::X509Credential(const std::string& cert_file,const std::string& key_file,const std::string& inpwd):key_(NULL),cert_(NULL),chain_(NULL) {
  EVP_PKEY *pkey = NULL;
  X509 *cert = NULL;
  STACK_OF(X509) *cert_sk = NULL;
  bool res = false;

  
//  OpenSSLInit();
  EVP_add_digest(EVP_sha256());

  if(!string_to_x509(cert_file,key_file,inpwd,cert,pkey,cert_sk)) goto err;
  cert_=cert; cert=NULL;
  key_=pkey; pkey=NULL;
  chain_=cert_sk; cert_sk=NULL;
  res=true;
err:
  if(!res) LogError();
  if(pkey) EVP_PKEY_free(pkey);
  if(cert) X509_free(cert);
  if(cert_sk) {
    for(int i = 0;i<sk_X509_num(cert_sk);++i) {
      X509* v = sk_X509_value(cert_sk,i);
      if(v) X509_free(v);
    };
    sk_X509_free(cert_sk);
  };
}

std::string X509Credential::Delegate(const std::string& request,const DelegationRestrictions& restrictions) {
  X509 *cert = NULL;
  X509_REQ *req = NULL;
  BIO* in = NULL;
  std::string res;

  // Unify format of request
  std::string prequest = request;
  strip_PEM_request(prequest);
  wrap_PEM_request(prequest);

  in = BIO_new_mem_buf((void*)(prequest.c_str()),prequest.length());
  if(!in) goto err;

  if((!PEM_read_bio_X509_REQ(in,&req,NULL,NULL)) || (!req)) goto err;
  BIO_free_all(in); in=NULL;

  cert = Delegate(req, restrictions);
  if(!cert) goto err;

  if(!x509_to_string(cert,res)) { res=""; goto err; };
  // Append chain of certificates
  if(!x509_to_string((X509*)cert_,res)) { res=""; goto err; };
  if(chain_) {
    for(int n=0;n<sk_X509_num((STACK_OF(X509) *)chain_);++n) {
      X509* v = sk_X509_value((STACK_OF(X509) *)chain_,n);
      if(!v) { res=""; goto err; };
      if(!x509_to_string(v,res)) { res=""; goto err; };
    };
  };

err:
  if(res.empty()) LogError();
  if(in) BIO_free_all(in);
  if(req) X509_REQ_free(req);
  if(cert) X509_free(cert);
  return res;
}

BIO* X509Credential::Delegate(BIO* request, const DelegationRestrictions& restrictions) {
  bool res = false;
  X509 *cert = NULL;
  X509_REQ *req = NULL;
  BIO* out = NULL;

  if((!d2i_X509_REQ_bio(request,&req)) || (!req)) goto err;

  cert = Delegate(req, restrictions);
  if(!cert) goto err;

  out = BIO_new(BIO_s_mem());
  if(!i2d_X509_bio(out, cert)) goto err;
  if(!i2d_X509_bio(out, (X509*)cert_)) goto err;
  if(chain_) {
    for(int n=0;n<sk_X509_num((STACK_OF(X509) *)chain_);++n) {
      X509* v = sk_X509_value((STACK_OF(X509) *)chain_,n);
      if(!v) goto err;
      if(!i2d_X509_bio(out, v)) goto err;
    };
  };

  res = true;

err:
  if(!res) LogError();
  if(req) X509_REQ_free(req);
  if(cert) X509_free(cert);
  if(!res && out) {
    BIO_free_all(out);
    out = NULL;
  }
  return out;
}

X509* X509Credential::Delegate(X509_REQ* request, const DelegationRestrictions& restrictions) {
  bool res = false;
  X509 *cert = NULL;
  EVP_PKEY *pkey = NULL;
  ASN1_INTEGER *sno = NULL;
  ASN1_OBJECT *obj= NULL;
  ASN1_OCTET_STRING* policy_string = NULL;
  //X509_EXTENSION *ex = NULL;
  PROXY_CERT_INFO_EXTENSION proxy_info;
  PROXY_POLICY proxy_policy;
  const EVP_MD *digest = EVP_sha256();
  X509_NAME *subject = NULL;
  char need_ext[] = "critical,digitalSignature,keyEncipherment";
  std::string proxy_cn;
  time_t validity_start_adjustment = 300; //  5 minute grace period for unsync clocks
  time_t validity_start = time(NULL);
  time_t validity_end = (time_t)(-1);
  DelegationRestrictions& restrictions_ = (DelegationRestrictions&)restrictions;
  std::string proxyPolicy;
  std::string proxyPolicyFile;

  if(!cert_) {
    dprintf(D_ALWAYS, "Missing certificate chain\n");
    return NULL;
  };
  if(!key_) {
    dprintf(D_ALWAYS, "Missing private key\n");
    return NULL;
  };

  //subject=X509_REQ_get_subject_name(request);
  //char* buf = X509_NAME_oneline(subject, 0, 0);
  //dprintf(D_FULLDEBUG, "subject=%s\n", buf);
  //OPENSSL_free(buf);

  if((pkey=X509_REQ_get_pubkey(request)) == NULL) goto err;
  if(X509_REQ_verify(request,pkey) <= 0) goto err;

  cert=X509_new();
  if(!cert) goto err;
  //ci=x->cert_info;
  sno = ASN1_INTEGER_new();
  if(!sno) goto err;
  // TODO - serial number must be unique among generated by proxy issuer
  if(!rand_serial(sno)) goto err;
  if (!X509_set_serialNumber(cert,sno)) goto err;
  proxy_cn=std::to_string(ASN1_INTEGER_get(sno));
  ASN1_INTEGER_free(sno); sno=NULL;
  X509_set_version(cert,2L);

  /*
   Proxy certificates do not need KeyUsage extension. But
   some old software still expects it to be present.

   From RFC3820:

   If the Proxy Issuer certificate has the KeyUsage extension, the
   Digital Signature bit MUST be asserted.
  */

  X509_add_ext_by_nid(cert,NID_key_usage,(char*)need_ext,-1);

  /*
   From RFC3820:

   If a certificate is a Proxy Certificate, then the proxyCertInfo
   extension MUST be present, and this extension MUST be marked as
   critical.
  
   The pCPathLenConstraint field, if present, specifies the maximum
   depth of the path of Proxy Certificates that can be signed by this
   Proxy Certificate. 

   The proxyPolicy field specifies a policy on the use of this
   certificate for the purposes of authorization.  Within the
   proxyPolicy, the policy field is an expression of policy, and the
   policyLanguage field indicates the language in which the policy is
   expressed.

   *  id-ppl-inheritAll indicates that this is an unrestricted proxy
      that inherits all rights from the issuing PI.  An unrestricted
      proxy is a statement that the Proxy Issuer wishes to delegate all
      of its authority to the bearer (i.e., to anyone who has that proxy
      certificate and can prove possession of the associated private
      key).  For purposes of authorization, this an unrestricted proxy
      effectively impersonates the issuing PI.

   *  id-ppl-independent indicates that this is an independent proxy
      that inherits no rights from the issuing PI.  This PC MUST be
      treated as an independent identity by relying parties.  The only
      rights this PC has are those granted explicitly to it.
  */
  /*
  ex=X509V3_EXT_conf_nid(NULL,NULL,NID_proxyCertInfo,"critical,CA:FALSE");
  if(!ex) goto err;
  if(!X509_add_ext(cert,ex,-1)) goto err;
  X509_EXTENSION_free(ex); ex=NULL;
  */
  memset(&proxy_info,0,sizeof(proxy_info));
  memset(&proxy_policy,0,sizeof(proxy_policy));
  proxy_info.pcPathLengthConstraint=NULL;
  proxy_info.proxyPolicy=&proxy_policy;
  proxy_policy.policyLanguage=NULL;
  proxy_policy.policy=NULL;
  proxyPolicy=restrictions_["proxyPolicy"];
  proxyPolicyFile=restrictions_["proxyPolicyFile"];
  if(!proxyPolicyFile.empty()) {
    if(!proxyPolicy.empty()) goto err; // Two policies supplied
    std::ifstream is(proxyPolicyFile.c_str());
    std::getline(is,proxyPolicy,(char)0);
    if(proxyPolicy.empty()) goto err;
  };
  if(!proxyPolicy.empty()) {
    obj=OBJ_nid2obj(NID_id_ppl_anyLanguage);  // Proxy with policy
    if(!obj) goto err;
    policy_string=ASN1_OCTET_STRING_new();
    if(!policy_string) goto err;
    ASN1_OCTET_STRING_set(policy_string,(const unsigned char*)(proxyPolicy.c_str()),proxyPolicy.length());
    proxy_policy.policyLanguage=obj;
    proxy_policy.policy=policy_string;
  } else {
    // The presence of a "policyLimited" restriction indicates to create a
    // limited proxy. The value doesn't matter.
    bool limited=restrictions_.find("policyLimited")!=restrictions_.end();
    PROXY_CERT_INFO_EXTENSION *pci =
        (PROXY_CERT_INFO_EXTENSION*)X509_get_ext_d2i((X509*)cert_,NID_proxyCertInfo,NULL,NULL);
    if(pci) {
      if(pci->proxyPolicy && pci->proxyPolicy->policyLanguage) {
        int const bufSize = 255;
        char* buf = new char[bufSize+1];
        int l = OBJ_obj2txt(buf,bufSize,pci->proxyPolicy->policyLanguage,1);
        if(l > 0) {
          if(l > bufSize) l=bufSize;
          buf[l] = 0;
          if(strcmp(GLOBUS_LIMITED_PROXY_OID,buf) == 0) {
            // Gross hack for globus. If Globus marks own proxy as limited
            // it expects every derived proxy to be limited or at least
            // independent. Independent proxies has little sense in Grid
            // world. So here we make our proxy globus-limited to allow
            // it to be used with globus code.
            limited = true;
          };
        };
        delete[] buf;
      };
      PROXY_CERT_INFO_EXTENSION_free(pci);
    };
    if(limited) {
      obj=OBJ_txt2obj(GLOBUS_LIMITED_PROXY_OID,1); // Limited proxy
    } else {
      obj=OBJ_nid2obj(NID_id_ppl_inheritAll);  // Unrestricted proxy
    };
    if(!obj) goto err;
    proxy_policy.policyLanguage=obj;
  };
  if(X509_add1_ext_i2d(cert,NID_proxyCertInfo,&proxy_info,1,X509V3_ADD_REPLACE) != 1) goto err;
  if(policy_string) { ASN1_OCTET_STRING_free(policy_string); policy_string=NULL; }
  ASN1_OBJECT_free(obj); obj=NULL;
  /*
  PROXY_CERT_INFO_EXTENSION *pci = X509_get_ext_d2i(x, NID_proxyCertInfo, NULL, NULL);
  typedef struct PROXY_CERT_INFO_EXTENSION_st {
        ASN1_INTEGER *pcPathLengthConstraint;
        PROXY_POLICY *proxyPolicy;
        } PROXY_CERT_INFO_EXTENSION;
  typedef struct PROXY_POLICY_st {
        ASN1_OBJECT *policyLanguage;
        ASN1_OCTET_STRING *policy;
        } PROXY_POLICY;
  */

  subject=X509_get_subject_name((X509*)cert_);
  if(!subject) goto err;
  subject=X509_NAME_dup(subject);
  if(!subject) goto err;
  if(!X509_set_issuer_name(cert,subject)) goto err;
  if(!X509_NAME_add_entry_by_NID(subject,NID_commonName,MBSTRING_ASC,(unsigned char*)(proxy_cn.c_str()),proxy_cn.length(),-1,0)) goto err;
  if(!X509_set_subject_name(cert,subject)) goto err;
  X509_NAME_free(subject); subject=NULL;
  // This was changed from the original to expect only unix epoch time values.
  if(!(restrictions_["validityStart"].empty())) {
    validity_start=atoll(restrictions_["validityStart"].c_str());
    validity_start_adjustment = 0;
  };
  if(!(restrictions_["validityEnd"].empty())) {
    validity_end=atoll(restrictions_["validityEnd"].c_str());
  } else if(!(restrictions_["validityPeriod"].empty())) {
    validity_end=validity_start+atoll(restrictions_["validityPeriod"].c_str());
  };
  validity_start -= validity_start_adjustment;
  //Set "notBefore"
  if( X509_cmp_time(X509_getm_notBefore((X509*)cert_), &validity_start) < 0) {
    X509_time_adj(X509_getm_notBefore(cert), 0L, &validity_start);
  }
  else {
    X509_set1_notBefore(cert, X509_getm_notBefore((X509*)cert_));
  }
  //Set "not After"
  if(validity_end == (time_t)(-1)) {
    X509_set1_notAfter(cert,X509_getm_notAfter((X509*)cert_));
  } else {
    X509_gmtime_adj(X509_getm_notAfter(cert), (validity_end-time(NULL)));
  };
  X509_set_pubkey(cert,pkey);
  EVP_PKEY_free(pkey); pkey=NULL;

  if(!X509_sign(cert,(EVP_PKEY*)key_,digest)) goto err;
  /*
  {
    int pci_NID = NID_undef;
    ASN1_OBJECT * extension_oid = NULL;
    int nid;
    PROXY_CERT_INFO_EXTENSION* proxy_cert_info;
    X509_EXTENSION *                    extension;

    pci_NID = OBJ_sn2nid(SN_proxyCertInfo);
    for(i=0;i<X509_get_ext_count(cert);i++) {
        extension = X509_get_ext(cert,i);
        extension_oid = X509_EXTENSION_get_object(extension);
        nid = OBJ_obj2nid(extension_oid);
        if(nid == pci_NID) {
            CleanError();
            if((proxy_cert_info = (PROXY_CERT_INFO_EXTENSION*)(X509V3_EXT_d2i(extension))) == NULL) {
              goto err;
              dprintf(D_ALWAYS, "X509V3_EXT_d2i failed\n");
            }
            break;
        }
    }
  }
  */

  res = true;

err:
  if(!res) LogError();
  if(pkey) EVP_PKEY_free(pkey);
  if(!res && cert) {
    X509_free(cert);
    cert = NULL;
  }
  if(sno) ASN1_INTEGER_free(sno);
  //if(ex) X509_EXTENSION_free(ex);
  if(obj) ASN1_OBJECT_free(obj);
  if(subject) X509_NAME_free(subject);
  if(policy_string) ASN1_OCTET_STRING_free(policy_string);
  return cert;
}


