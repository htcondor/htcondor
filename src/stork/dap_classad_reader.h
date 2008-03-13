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

#ifndef _DAP_CLASSAD_READER_H_
#define _DAP_CLASSAD_READER_H_

#include "condor_common.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "dap_constants.h"

#define WANT_CLASSAD_NAMESPACE
#include "condor_fix_iostream.h"
#include "classad/classad_distribution.h"

class ClassAd_Reader{
  FILE *adfile;
  classad::ClassAd *currentAd;

 public:
  ClassAd_Reader(const char *fname);
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



