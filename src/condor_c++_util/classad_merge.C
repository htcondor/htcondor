/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"
#include "condor_classad.h"
#include "classad_merge.h"

void MergeClassAds(ClassAd *merge_into, ClassAd *merge_from, 
				   bool merge_conflicts)
{
	merge_from->ResetName();
	merge_from->ResetExpr();

	while (1) {
		const char     *name;
		ExprTree       *expression;

		name       = merge_from->NextNameOriginal();
		expression = merge_from->NextExpr();

		if (name == NULL || expression == NULL) {
			break;
		} else {
			if (merge_conflicts || !merge_into->Lookup(name)) {
				ExprTree  *copy_expression;

				copy_expression = expression->DeepCopy();
				merge_into->Insert(copy_expression);
			}
		}
	}

	return;
}
