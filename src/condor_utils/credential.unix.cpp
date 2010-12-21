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

#include "credential.unix.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "classad/classad_distribution.h"

Credential::Credential(const classad::ClassAd& class_ad) {
	std::string val;

	if (class_ad.EvaluateAttrString ((const char *)CREDATTR_NAME, val)) {
		name=val.c_str();
	}

	if (class_ad.EvaluateAttrString (CREDATTR_OWNER, val)) {
		owner=val.c_str();
	}

	class_ad.EvaluateAttrInt (CREDATTR_TYPE, type);
	class_ad.EvaluateAttrInt (CREDATTR_DATA_SIZE, m_data_size);
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
	class_ad->InsertAttr (CREDATTR_DATA_SIZE, m_data_size);

	return class_ad;
}

int
Credential::GetDataSize() {
	return m_data_size;
}

void
Credential::SetData (const void * buff, int buff_size) {
  if (data != NULL) {
	  free (data);
  }
  data = malloc(buff_size);
  memcpy (data, buff, buff_size);
  m_data_size = buff_size;
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
	m_data_size = size;
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
