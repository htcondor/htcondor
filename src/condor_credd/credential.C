#include "credential.h"

Credential::Credential(const classad::ClassAd& _class_ad) {
  attr_list = new classad::ClassAd (_class_ad);
}

Credential::Credential() {
  attr_list = new classad::ClassAd();
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

const classad::ClassAd * 
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
  std::string attrName(name);
  attr_list->InsertAttr(attrName, value);
}

int
Credential::GetIntAttribute (const char * name) {
  int value;
  if (!attr_list->EvaluateAttrInt( name, value))
    return 0;

  return value;

}

void
Credential::SetIntAttribute(const char * name, int value) {
  std::string attrName(name);
  attr_list->InsertAttr(attrName, value);
}

char *
Credential::GetStringAttribute (const char * name) {
  char * value;
#if 0
  if (!attr_list->LookupString (name, &value))
    return NULL;

  return value;
#endif

  // plagiarized from deprecated
  // ClassAd::LookupString (const char *name, char **value) const
  std::string strName(name), strVal;
  if( !attr_list->EvaluateAttrString( strName, strVal ) ) {
	  return NULL;
  }
  const char *strValCStr = strVal.c_str( );
  value = (char *) malloc(strlen(strValCStr) + 1);
  if (value != NULL) {
    strcpy(value, strValCStr);
    return value;
  }
  return NULL;
}

