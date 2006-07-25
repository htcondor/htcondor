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
// This header file is NOT for a C/C++ compiler; it is for the 
// gSOAP stub generator.

/*
 Below are all the xsd schema types that are required. When gSOAP
 parses this file it will see types of for form x__y, the x will
 become the namespace and y will be the actual name of the type.

 gSOAP also recognizes structs with values "*__ptr" and "__size" as
 representing an array type.

 The numbers after a struct's member's name is
 MIN_OCCURRANCES:MAX_OCCURRANCES.
 */

//gsoap condor service namespace: urn:condor
//gsoap condor schema form: unqualified

typedef char *xsd__string;
typedef char *xsd__anyURI;
typedef float xsd__float;
typedef int xsd__int;
typedef bool xsd__boolean;
typedef long long xsd__long;
typedef char xsd__byte;
struct xsd__base64Binary
{
  unsigned char * __ptr;
  int __size;
};

enum condor__StatusCode
{
  SUCCESS,
  FAIL,
  INVALIDTRANSACTION,
  UNKNOWNCLUSTER,
  UNKNOWNJOB,
  UNKNOWNFILE,
  INCOMPLETE,
  INVALIDOFFSET,
  ALREADYEXISTS
};

struct condor__Status
{
  enum condor__StatusCode code 1:1;
  xsd__string message 0:1;
  struct condor__Status *next 0:1;
};

enum condor__ClassAdAttrType
{
  INTEGER_ATTR = 'n',
  FLOAT_ATTR = 'f',
  STRING_ATTR = 's',
  EXPRESSION_ATTR = 'x',
  BOOLEAN_ATTR = 'b',
  UNDEFINED_ATTR = 'u',
  ERROR_ATTR = 'e'
};

// n=int,f=float,s=string,x=expression,b=bool,u=undefined,e=error
struct condor__ClassAdStructAttr
{
  xsd__string name 1:1;
  //	xsd__byte type 1:1;
  enum condor__ClassAdAttrType type 1:1;
  xsd__string value 1:1;
};

struct condor__ClassAdStruct
{
	struct condor__ClassAdStructAttr *__ptr;	
	int __size;
};

struct condor__ClassAdStructArray 
{
	struct condor__ClassAdStruct *__ptr;
	int __size;
};

struct condor__ClassAdStructAndStatus
{
  struct condor__Status status 1:1;
  struct condor__ClassAdStruct classAd 0:1;
};

struct condor__ClassAdStructArrayAndStatus
{
  struct condor__Status status 1:1;
  struct condor__ClassAdStructArray classAdArray 0:1;
};

struct condor__StringAndStatus
{
  struct condor__Status status 1:1;
  xsd__string message 0:1;
};
