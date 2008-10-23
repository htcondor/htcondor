/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CLASSAD_REEVALUATOR_H
#define _CLASSAD_REEVALUATOR_H

#include "condor_classad.h"

/**
 * Reevaluate, distructively, the offer ClassAd in the context of the
 * request ClassAd, based on configured policy. For instance,
 * 
 *   REEVALUATE_ATTRIBUTES = CurMatches
 *   REEVALUATE_CURMATCHES_EXPR = MY.CurMatches + 1
 * 
 *    results in: offer["CurMatches"] = offer["CurMatches"] + 1
 * 
 *  or,
 *
 *   REEVALUATE_ATTRIBUTES = AvailableMemory
 *   REEVALUATE_AVAILABLEMEMORY_EXPR = MY.AvailableMemory - TARGET.RequestedMemory
 *
 *    results in: offer["AvailableMemory"] = 
 *                   offer["AvailableMemory"] - request["RequestedMemory"]
 *
 * This function is very useful as part of the negotiation cycle to
 * allow for matching a single machine ad multiple times, but with
 * ever fewer resources provided by the machine on each match.
 *
 * Another potential expression would be to decrement available slots
 * on the machine represented by the offer ad, e.g.
 *
 *   REEVALUATE_ATTRIBUTES = Slots
 *   REEVALUATE_SLOTS_EXPR = MY.Slots - TARGET.RequestedSlots
 *
 * Generally configuration is as follows:
 *
 *   REEVALUATE_ATTRIBUTES = A, B, C
 *   REEVALUATE_A_EXPR = ...
 *   REEVALUATE_B_EXPR = ...
 *   REEVALUATE_C_EXPR = ...
 *
 * Lack of a REEVALUATE_*_EXPR configuration, or an error while
 * evaluating the expression, will result in a failure of this
 * function, i.e. false will be returned. On failure, the offer ad may
 * be in an inconsistent state and should not be used further, e.g. in
 * the general example above A and B may be reevaluated but C may
 * result in an error meaning that A and B are changed by C is
 * not. Also, the attribute to be updated must also exist, because its
 * type must be known so the proper AttrList::Eval function can be
 * called. If the attribute does not exist in the offer ad false is
 * returned.
 */

bool classad_reevaluate(ClassAd *ad, const ClassAd *context);

#endif /* _CLASSAD_REEVALUATOR_H */
