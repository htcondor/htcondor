#ifndef __CREDENTIAL_H__
#define __CREDENTIAL_H__

#define CREDENTIAL_METADATA_SEPARATOR ' '

#define X509_CREDENTIAL_TYPE 1
#define PASSWORD_CREDENTIAL_TYPE 2

#include "condor_common.h"
#include <time.h>
#include "condor_classad.h"


class Credential {

public:
  Credential(const ClassAd&);
  Credential();
  virtual ~Credential();

  time_t GetExpirationTime();
  virtual time_t GetRealExpirationTime() = 0;
  
  const char * GetName();
  void SetName(const char *);

  int GetType();

  const char * GetOwner();
  void SetOwner (const char *);

  const char * GetOrigOwner();
  void SetOrigOwner(const char *);

  const ClassAd * GetClassAd();
 
  virtual int GetDataSize();
  virtual void SetData(const void * pData, int size);
  virtual int GetData(void*& pData, int & size);

  virtual void SetStorageName(const char *) = 0;
  virtual const char * GetStorageName() = 0;

 protected:
  ClassAd * attr_list;
  void * data;

  void SetDataSize(int);

  void SetStringAttribute (const char* name, const char * value);
  char * GetStringAttribute (const char * name);

  void SetIntAttribute (const char * name, int value);
  int GetIntAttribute (const char * name);

  MyString name;
  MyString owner;
  MyString orig_owner;

};

#endif
