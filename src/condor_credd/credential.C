#include "credential.h"

Credential::Credential(const ClassAd& _class_ad) {
  attr_list = new ClassAd (_class_ad);
}

Credential::Credential() {
  attr_list = new ClassAd();
}

Credential::~Credential() {
  if (attr_list)
    delete attr_list;
}

const char * 
Credential::GetName() {
  char * buff = GetStringAttribute ("Name");
  name = buff;
  free (buff);
  return name.GetCStr();
}

void 
Credential::SetName(const char * _name) {
  name = _name;
  SetStringAttribute ("Name", _name);
}

const ClassAd * 
Credential::GetClassAd() {
  return attr_list;

}

int
Credential::GetDataSize() {
  return GetIntAttribute ("DataSize");
}

void
Credential::SetData (const void * buff, int buff_size) {
  data = malloc(buff_size);
  memcpy (data, buff, buff_size);
  SetDataSize (buff_size);
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
  SetIntAttribute ("DataSize", size);
}

void
Credential::SetOrigOwner (const char * _owner) {
  orig_owner = _owner;
  SetStringAttribute ("OriginalOwner", _owner);
}

const char *
Credential::GetOrigOwner() {
  char * buff = GetStringAttribute("OriginalOwner");
  orig_owner = buff;
  free (buff);
  return orig_owner.GetCStr();
}

void
Credential::SetOwner (const char * _owner) {
  owner = _owner;
  SetStringAttribute ("Owner", _owner);
}

const char *
Credential::GetOwner() {
  char * buff = GetStringAttribute("Owner");
  owner = buff;
  free (buff);
  return owner.GetCStr();
}

int
Credential::GetType() {
  return GetIntAttribute ("Type");
}

void
Credential::SetStringAttribute(const char * name, const char * value) {
  char buff[10000];
  sprintf (buff, "%s = \"%s\"", name, value);
  attr_list->InsertOrUpdate (buff);
}

int
Credential::GetIntAttribute (const char * name) {
  int value;
  if (!attr_list->LookupInteger (name, value))
    return 0;

  return value;

}

void
Credential::SetIntAttribute(const char * name, int value) {
  char buff[10000];
  sprintf (buff, "%s = %d", name, value);
  attr_list->InsertOrUpdate (buff);
}

char *
Credential::GetStringAttribute (const char * name) {
  char * value;
  if (!attr_list->LookupString (name, &value))
    return NULL;

  return value;

}
