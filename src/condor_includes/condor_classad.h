/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef __CLASSAD_PACKAGE_H__
#define __CLASSAD_PACKAGE_H__

#include "../condor_classad.V6/common.h"
#include "../condor_classad.V6/exprTree.h"
#include "../condor_classad.V6/matchClassad.h"
#include "../condor_classad.V6/sink.h"
#include "../condor_classad.V6/source.h"
#include "../condor_classad.V6/xmlSource.h"
#include "../condor_classad.V6/xmlSink.h"

#if defined( COLLECTIONS )
#include "../condor_classad.V6/collectionServer.h"
#include "../condor_classad.V6/collectionClient.h"
#include "../condor_classad.V6/query.h"
#else
#include "../condor_classad.V6/noCollections.h"
#endif

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
bool putOldClassAdNoTypes( Stream *, ClassAd& );  // NAC

static const int ATTRLIST_MAX_EXPRESSION = 10240;

END_NAMESPACE

#endif
