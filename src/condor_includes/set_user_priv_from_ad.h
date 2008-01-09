/* This file is implemented in condor_c++_util/set_user_priv_from_ad.C */

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
priv_state set_user_priv_from_ad(ClassAd const &ad);

#endif
