
#ifndef _SHIP_FNS_H_
#define _SHIP_FNS_H_

#include "shipobject.h"

//
// Creates a new shipping object related to the parent object by
// a relationship named attrName
//
Ship_Obj *CreateObjAttr(Ship_ComplexObj *parent,
			const char *attrName,
			const char *attrType,
			const char *attrValue);
			
//
// Creates a new shipping object related to the parent object by
// a relationship named attrName. Assumes that the type of the
// child object should be "integer".			
//
Ship_Obj *CreateObjAttr(Ship_ComplexObj *parent,
			const char *attrName,
			int attrValue);

//
// Returns true if type is one of "integer", "real", or "string"
//
bool IsPrimitiveType(const char *type);

//
// Finds the object related to parent by the relationship named
// attrName, and sets the value of this child object to attrValue.
// If the relationship is multi-valued, it will only change the value
// of the first child.
//
// Returns false if no related object exists
//
bool SetObjAttr(Ship_ComplexObj *parent,
		const char *attrName,
		const char *attrValue);

//
// Same as above, but assumes the type of the child object is "integer"
//
bool SetObjAttr(Ship_ComplexObj *parent,
		const char *attrName,
		int attrValue);

//
// Finds the object related to parent by the relationship named
// attrName, and sets attrValue to point to the value of the
// child object. If the relationship is multi-valued, it will report
// the value of the first child.
//
// Returns false if no related object exists
//
bool GetObjAttr(Ship_ComplexObj *o,
		const char *attrName,
		char *&attrValue);
		
#endif

