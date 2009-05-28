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

#ifndef _SET_USER_FROM_AD_H_
#define _SET_USER_FROM_AD_H_

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "classad_newold.h"
#include "condor_uid.h"
#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"

void set_user_from_ad(classad::ClassAd const &ad)
{
        std::string owner;
        std::string domain;
        if( 0 == ad.EvaluateAttrString(ATTR_OWNER,owner) ) {
                classad::ClassAd ad_copy;
                ClassAd old_ad;
                ad_copy = ad;
                if(new_to_old(ad_copy,old_ad)) {
                        old_ad.dPrint(D_ALWAYS);
                }
                EXCEPT("Failed to find %s in job ad.",ATTR_OWNER);
        }
        ad.EvaluateAttrString(ATTR_NT_DOMAIN,domain);

        if( 0 == init_user_ids(owner.c_str(),domain.c_str()) ) {
                EXCEPT("Failed in init_user_ids(%s,%s)",owner.c_str(),domain.c_str());
        }

        return;
}

#endif
