/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __CLASSAD_PACKAGE_H__
#define __CLASSAD_PACKAGE_H__

#include "../condor_classad.V6/common.h"
#include "../condor_classad.V6/exprTree.h"
#include "../condor_classad.V6/matchClassad.h"
#include "../condor_classad.V6/collectionServer.h"
#include "../condor_classad.V6/collectionClient.h"
#include "../condor_classad.V6/query.h"

// The following code is only temporary, and should be removed after the
// rest of condor code starts explicitly using namespaces such as classad::
// and condor:: and dagman:: in class names and meathods.
#if defined( WANT_NAMESPACES )
using namespace classad;
#endif

#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"

BEGIN_NAMESPACE( classad )

void printClassAdExpr( ExprTree * );
void printClassAdValue( Value & );
ClassAd* getOldClassAd( Stream* );
bool getOldClassAd( Stream*, ClassAd& );
bool putOldClassAd( Stream*, ClassAd& );
bool getOldClassAdNoTypes( Stream *, ClassAd& );  // NAC

static const int ATTRLIST_MAX_EXPRESSION = 10240;

END_NAMESPACE

#endif
