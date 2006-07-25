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
#include "credential.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "classad_distribution.h"

Credential::Credential(const classad::ClassAd& class_ad) {
	std::string val;

	if (class_ad.EvaluateAttrString ((const char *)CREDATTR_NAME, val)) {
		name=val.c_str();
	}

	if (class_ad.EvaluateAttrString (CREDATTR_OWNER, val)) {
		owner=val.c_str();
	}

	class_ad.EvaluateAttrInt (CREDATTR_TYPE, type);
	class_ad.EvaluateAttrInt (CREDATTR_DATA_SIZE, data_size);
	data = NULL;
}

Credential::Credential() {
	data = NULL;
}

Credential::~Credential() {
	if (data != NULL) {
		free (data);
	}
}

const char * 
Credential::GetName() {
	return name.Value();
}

void 
Credential::SetName(const char * _name) {
	ASSERT (_name);
	name = _name;
}



classad::ClassAd * 
Credential::GetMetadata() {
	classad::ClassAd * class_ad = new classad::ClassAd();
	ASSERT (!name.IsEmpty());
	class_ad->InsertAttr (CREDATTR_NAME, name.Value());
	class_ad->InsertAttr (CREDATTR_TYPE, type);
	class_ad->InsertAttr (CREDATTR_OWNER, owner.Value());
	class_ad->InsertAttr (CREDATTR_DATA_SIZE, data_size);

	return class_ad;
}

int
Credential::GetDataSize() {
	return data_size;
}

void
Credential::SetData (const void * buff, int buff_size) {
  if (data != NULL) {
	  free (data);
  }
  data = malloc(buff_size);
  memcpy (data, buff, buff_size);
  data_size = buff_size;
}

int
Credential::GetData (void *& pData, int & size) {
  if (data == NULL)
    return FALSE;

  int data_size = GetDataSize();

  pData = malloc (data_size);
  memcpy (pData, data, data_size);
  size = data_size;
  return TRUE;
}

void
Credential::SetDataSize (int size) {
	data_size = size;
}

void
Credential::SetOrigOwner (const char * _owner) {
	ASSERT (_owner);
	orig_owner = _owner;
}

const char *
Credential::GetOrigOwner() {
	return orig_owner.Value();
}

void
Credential::SetOwner (const char * _owner) {
	owner = (_owner)?_owner:"";
}

const char *
Credential::GetOwner() {
	return owner.Value();
}

int
Credential::GetType() {
	return type;
}
