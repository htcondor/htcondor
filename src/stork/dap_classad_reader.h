#ifndef _DAP_CLASSAD_READER_H_
#define _DAP_CLASSAD_READER_H_

#include "condor_common.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dap_constants.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"

class ClassAd_Reader{
  FILE *adfile;
  classad::ClassAd *currentAd;

 public:
  ClassAd_Reader(char *fname);
  ~ClassAd_Reader();
  const classad::ClassAd * getCurrentAd () { return currentAd; }
  int readAd();
  void reset();
  int getValue(char *attr, char *attrval);
  int queryAttr(char *attr1, char* attrval1, char *attr2,char *attrval2 );
  classad::ClassAd* returncurrentAd();
  int lookFor(
          char *keyattr,
          const std::string keyval,
          char *attr,char *attrval
          );
  int returnFirst(const std::string qattr,
          char *qval,
          char *attr,
          char *attrval
          );
};

int get_subAdval(classad::ClassAd *classAd, char* subClassAd, char *attr, char* attrval);
int getValue(classad::ClassAd *currentAd, char *attr, char *attrval);

#endif



