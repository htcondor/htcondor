
#include "ShipFns.h"
#include "util.h"

bool
IsPrimitiveType(const char *type)
{
    if (!strcmp(type, "string"))
	return true;
    if (!strcmp(type, "integer"))
	return true;
    if (!strcmp(type, "real"))
	return true;
    return false;
}

bool
SetObjAttr(Ship_ComplexObj *obj,
		const char *attrName,
		const char *attrValue)
{
    List<OBJECTINDEX> *indexList;
    Node<OBJECTINDEX> *node;
    Ship_Obj *objPtr;
    Ship_ObjList *list;
    
    if ((indexList = obj->relatedobjs(attrName)) == NULL)
    {
	return false;
    }
    
    if ((node = indexList->getheader()) == NULL)
    {
	return false;
    }
    
    list = obj->GetList();
    objPtr = (*list)[node->getvalue()];
    objPtr->SetQual((char *) attrValue);
    
    return true;
    
}

bool
SetObjAttr(Ship_ComplexObj *obj, const char *attrName, int attrValue)
{
    bool status;
    char *value;
    value = IntToString(attrValue);
    status = SetObjAttr(obj, attrName, value);
    delete value;
    return status;
}

Ship_Obj *
CreateObjAttr(Ship_ComplexObj *parent, const char *attrName,
	      const char *attrType, const char *attrValue)
{
    Ship_Obj *child;
    if (IsPrimitiveType(attrType))
	child = new Ship_Obj(parent->GetList());
    else
	child = new Ship_ComplexObj(parent->GetList());
    if (child == NULL)
    {
	return NULL;
    }
    child->SetType((char *) attrType);
    child->SetQual((char *) attrValue);
    parent->Create_and_set((char *) attrName, child);
    return child;
}

Ship_Obj *
CreateObjAttr(Ship_ComplexObj *parent, const char *attrName, int attrValue)
{
    Ship_Obj *o;
    char *value;
    value = IntToString(attrValue);
    o = CreateObjAttr(parent, attrName, "integer", value);
    delete value;
    return o;
}

bool
GetObjAttr(Ship_ComplexObj *obj, const char *attrName, char *&attrValue)
{
    List<OBJECTINDEX> *indexList;
    Node<OBJECTINDEX> *node;
    Ship_Obj *objPtr;
    Ship_ObjList *list;
    
    if ((indexList = obj->relatedobjs(attrName)) == NULL)
    {
	return false;
    }
    
    if ((node = indexList->getheader()) == NULL)
    {
	return false;
    }
    
    list = obj->GetList();
    objPtr = (*list)[node->getvalue()];
    attrValue = objPtr->GetQual();
    
    return true;
    
}

