/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "clerk_utils.h"

bool read_classad_file(const char *filename, ClassAdList &classads, const char * constr)
{
	bool success = false;

	FILE* file = safe_fopen_wrapper_follow(filename, "r");
	if (file == NULL) {
		dprintf(D_ALWAYS, "Can't open file of job ads: %s\n", filename);
		return false;
	} else {
		CondorClassAdFileParseHelper parse_helper("\n");

		for (;;) {
			ClassAd* classad = new ClassAd();

			int error;
			bool is_eof;
			int cAttrs = classad->InsertFromFile(file, is_eof, error, &parse_helper);

			bool include_classad = cAttrs > 0 && error >= 0;
			if (include_classad && constr) {
				classad::Value val;
				if (classad->EvaluateExpr(constr,val)) {
					if ( ! val.IsBooleanValueEquiv(include_classad)) {
						include_classad = false;
					}
				}
			}
			if (include_classad) {
				classads.Insert(classad);
			} else {
				delete classad;
			}

			if (is_eof) {
				success = true;
				break;
			}
			if (error < 0) {
				success = false;
				break;
			}
		}

		fclose(file);
	}
	return success;
}


