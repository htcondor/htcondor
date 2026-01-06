/***************************************************************
 *
 * Copyright (C) 1990-2023, Condor Team, Computer Sciences Department,
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

#ifndef __CLASSAD_INLINE_EVAL_H__
#define __CLASSAD_INLINE_EVAL_H__

#include "classad/classad_inline_value.h"
#include "classad/value.h"

namespace classad {

// Fast-path evaluation for inline values
// This is used to optimize EvaluateAttr when the requested attribute
// contains an inline value, avoiding the full Evaluate() machinery

// Try to extract a value from an inline representation
// Returns true if val is inline and extraction succeeded
inline bool tryExtractInlineValue(ExprTree* ptr, Value& outVal, 
                                  const InlineStringBuffer* stringBuffer = nullptr) {
	InlineValue val(ptr);
	if (!val.isInline()) {
		return false;
	}
	return inlineValueToValue(val, outVal, stringBuffer);
}

// Unparse an inline value directly to a string buffer
// Returns true if ptr contains an inline value that was successfully unparsed
// This is an internal optimization - external APIs should use Unparse(ExprTree*)
bool unparseInlineValue(ExprTree* ptr, std::string& buffer, 
                        const InlineStringBuffer* stringBuffer = nullptr);

} // namespace classad

#endif // __CLASSAD_INLINE_EVAL_H__
