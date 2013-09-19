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

/* This file is implemented in condor_utils/set_user_priv_from_ad.C */

#ifndef _SET_USER_PRIV_FROM_AD_H_
#define _SET_USER_PRIV_FROM_AD_H_

#include "condor_classad.h"
#include "condor_uid.h"

//
// Set the user priv based on the ATTR_OWNER and ATTR_NT_DOMAIN from
// the given ClassAd
//
// EXCEPT is invoked if the given ad is missing its ATTR_OWNER or if
// init_user_ids with the owner and domain from the ad fails
//
priv_state set_user_priv_from_ad(classad::ClassAd const &ad);

#endif
