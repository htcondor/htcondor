/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __CREDENTIAL_H__
#define __CREDENTIAL_H__

#define CREDENTIAL_METADATA_SEPARATOR ' '

#define X509_CREDENTIAL_TYPE 1
#define PASSWORD_CREDENTIAL_TYPE 2

#define DEFAULT_CREDENTIAL_NAME "DEFAULT"

#include "condor_common.h"
#include <time.h>
#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"

#include "MyString.h"

#define CREDATTR_NAME "Name"
#define CREDATTR_TYPE "Type"
#define CREDATTR_OWNER "Owner"
#define CREDATTR_ORIG_OWNER "OrigOwner"
#define CREDATTR_DATA_SIZE "DataSize"

class Credential {

public:
  Credential(const classad::ClassAd&);
  Credential();
  virtual ~Credential();

  virtual time_t GetRealExpirationTime() = 0;
  virtual void SetRealExpirationTime(time_t) = 0;

  const char * GetName();
  void SetName(const char *);

  int GetType();
  virtual const char * GetTypeString() = 0;

  const char * GetOwner();
  void SetOwner (const char *);

  const char * GetOrigOwner();
  void SetOrigOwner(const char *);

  virtual classad::ClassAd * GetMetadata();
 
  virtual int GetDataSize();
  virtual void SetData(const void * pData, int size);
  virtual int GetData(void*& pData, int & size);


 protected:
  MyString name;
  int type;
  MyString owner;
  MyString orig_owner;

  void * data;
  int data_size;

  void SetDataSize(int);


};

#endif
