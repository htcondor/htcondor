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

#include "condor_distribution.h"

#include "condor_classad.h"
#include "string_list.h"
#include "MyString.h"
#include "condor_debug.h"
#include "condor_config.h"


char *mySubSystem = "TEST";


int
main(int argc, char **argv)
{
	ClassAd ad, context;
	char *str;
	int i;
	float f;
	bool b;

	myDistro->Init(argc, argv);
	config();

	dprintf_set_tool_debug("TEST", 0);

	ad.Assign("REEVALUATE_ATTRIBUTES", "INTEGER, FLOAT, STRING, BOOL, REF");
	ad.AssignExpr("REEVALUATE_INTEGER_EXPR", "MY.INTEGER + 1");
	ad.AssignExpr("REEVALUATE_FLOAT_EXPR", "MY.FLOAT + 0.1");
	ad.AssignExpr("REEVALUATE_STRING_EXPR", "strcat(MY.STRING, \",\", MY.STRING)");
	ad.AssignExpr("REEVALUATE_BOOL_EXPR", "MY.BOOL && FALSE");
	ad.AssignExpr("REEVALUATE_REF_EXPR", "MY.REF + TARGET.REF");

	ad.Assign("INTEGER", 1);
	ad.Assign("FLOAT", 1.0);
	ad.Assign("STRING", "Hello");
	ad.Assign("BOOL", true);
	ad.Assign("REF", 3);

	context.Assign("REF", 2);

	dprintf(D_FULLDEBUG, "context ad:\n");
	context.dPrint(D_FULLDEBUG);
	dprintf(D_FULLDEBUG, "ad to update:\n");
	ad.dPrint(D_FULLDEBUG);

	if (!classad_reevaluate(&ad, &context)) {
		return 1;
	}

	dprintf(D_FULLDEBUG, "updated ad:\n");
	ad.dPrint(D_FULLDEBUG);

	if (ad.LookupInteger("INTEGER", i)) {
		if (2 != i) {
			return 2;
		}
	} else {
		return 3;
	}

	if (ad.LookupFloat("FLOAT", f)) {
		if (1.1f != f) {
			return 4;
		}
	} else {
		return 5;
	}

	if (ad.LookupString("STRING", &str)) {
		if (strcmp(str, "Hello,Hello")) {
			free(str);

			return 6;
		}

		free(str);
	} else {
		return 7;
	}

	if (ad.LookupBool("BOOL", b)) {
		if (b) {
			return 8;
		}
	} else {
		return 9;
	}

	if (ad.LookupInteger("REF", i)) {
		if (5 != i) {
			return 10;
		}
	} else {
		return 11;
	}

	return 0;
}
