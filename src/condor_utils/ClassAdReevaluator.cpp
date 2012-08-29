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

#include "condor_common.h"

#include "ClassAdReevaluator.h"

#include "condor_classad.h"
#include "string_list.h"
#include "MyString.h"
#include "condor_debug.h"

bool
classad_reevaluate(ClassAd *ad, ClassAd *context)
{
	StringList *reevaluate_attrs;
	MyString stmp;
	char *ptmp, *atmp, *ntmp = NULL;
	ExprTree *etmp;
	int itmp;
	float ftmp;
	
	if (!ad->LookupString("REEVALUATE_ATTRIBUTES", &ptmp)) {
		dprintf(D_FULLDEBUG,
				"classad_reevaluate: REEVALUATE_ATTRIBUTES not defined, skipping\n");

		return true;
	}

	reevaluate_attrs = new StringList(ptmp);
	if (!reevaluate_attrs) {
		dprintf(D_ALWAYS,
				"classad_reevaluate: Failed to parse REEVALUATE_ATTRS: %s\n",
				ptmp);

		goto FAIL;
	}

	free(ptmp);
	ptmp = NULL;

	reevaluate_attrs->rewind();
	while (NULL != (atmp = reevaluate_attrs->next())) {
		stmp.formatstr("REEVALUATE_%s_EXPR", atmp);

		dprintf(D_FULLDEBUG,
				"classad_reevaluate: Attempting reevaluate %s with %s\n",
				atmp, stmp.Value());

		etmp = ad->LookupExpr(atmp);
		if (!etmp) {
			dprintf(D_ALWAYS,
					"classad_reevaluate: %s does not exist in ad, returning\n",
					atmp);

			goto FAIL;
		}

		if ( ad->LookupString(atmp, &ntmp) ) {
			free(ntmp);
			ntmp = NULL;

			if (!ad->EvalString(stmp.Value(), context, &ntmp)) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to evaluate %s as a String\n",
						stmp.Value());

				goto FAIL;
			}

			if (!ad->Assign(atmp, ntmp)) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to assign new value %s to %s\n",
						ntmp, atmp);

				goto FAIL;
			}

			dprintf(D_FULLDEBUG,
					"classad_reevaluate: Updated %s to %s\n",
					atmp, ntmp);

			free(ntmp);
			ntmp = NULL;

		} else if ( ad->LookupInteger(atmp, itmp) ) {

			if (!ad->EvalInteger(stmp.Value(), context, itmp)) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to evaluate %s as an Integer\n",
						stmp.Value());

				goto FAIL;
			}

			if (!ad->Assign(atmp, itmp)) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to assign new value %d to %s\n",
						itmp, atmp);

				goto FAIL;
			}

			dprintf(D_FULLDEBUG,
					"classad_reevaluate: Updated %s to %d\n",
					atmp, itmp);

		} else if ( ad->LookupFloat(atmp, ftmp) ) {

			if (!ad->EvalFloat(stmp.Value(), context, ftmp)) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to evaluate %s as a Float\n",
						stmp.Value());

				goto FAIL;
			}

			if (!ad->Assign(atmp, ftmp)) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to assign new value %f to %s\n",
						ftmp, atmp);

				goto FAIL;
			}

			dprintf(D_FULLDEBUG,
					"classad_reevaluate: Updated %s to %f\n",
					atmp, ftmp);

		} else if ( ad->LookupBool(atmp, itmp) ) {

			if (!ad->EvalBool(stmp.Value(), context, itmp)) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to evaluate %s as a Bool\n",
						stmp.Value());

				goto FAIL;
			}

			if (!ad->Assign(atmp, (itmp ? true : false))) {
				dprintf(D_ALWAYS,
						"classad_reevaluate: Failed to assign new value %d to %s\n",
						itmp, atmp);

				goto FAIL;
			}

			dprintf(D_FULLDEBUG,
					"classad_reevaluate: Updated %s to %d\n",
					atmp, itmp);

		} else {
			dprintf(D_ALWAYS,
					"classad_reevaluate: %s has an unsupported type\n, cannot reevaluate\n",
					atmp);
		}
	}

	delete reevaluate_attrs;

	return true;

 FAIL:

	if (reevaluate_attrs) delete reevaluate_attrs;
	if (ntmp) free(ntmp);

	return false;
}
