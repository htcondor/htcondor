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

#include "classad/classad_inline_value.h"
#include "classad/value.h"
#include "classad/literals.h"
#include "classad/exprTree.h"
#include "classad/classad.h"

namespace classad {

ExprTree* InlineExpr::materialize() const {
	if (!_value) {
		return nullptr;
	}
	// We need to call ClassAd::MaterializeInlineValue, but we only have ClassAdFlatMap*
	// The materialize method is in ClassAdFlatMap, so call it directly
	if (_value->isInline()) {
		if (!_attrList) {
			return nullptr;  // Can't materialize inline value without the map
		}
		return _attrList->materialize(*_value);
	} else {
		return _value->asPtr();
	}
}

bool tryMakeInlineValue(const Value& val, InlineExprStorage& outValue, InlineStringBuffer* stringBuffer) {
	switch (val.GetType()) {
		case Value::ERROR_VALUE:
			outValue = InlineExprStorage::makeErrorLiteral();
			return true;

		case Value::UNDEFINED_VALUE:
			outValue = InlineExprStorage::makeUndefinedLiteral();
			return true;

		case Value::BOOLEAN_VALUE: {
			bool b;
			if (val.IsBooleanValue(b)) {
				outValue = InlineExprStorage::makeBool(b);
				return true;
			}
			return false;
		}

		case Value::INTEGER_VALUE: {
			long long i;
			if (val.IsIntegerValue(i)) {
				// Only inline if it fits in int32
				if (i >= std::numeric_limits<int32_t>::min() && 
					i <= std::numeric_limits<int32_t>::max()) {
					outValue = InlineExprStorage::makeInt32(static_cast<int32_t>(i));
					return true;
				}
			}
			return false;
		}

		case Value::REAL_VALUE: {
			double d;
			if (val.IsRealValue(d)) {
				// Check if it can be represented as float32 without significant loss
				float f = static_cast<float>(d);
				outValue = InlineExprStorage::makeFloat32(f);
				return true;
			}
			return false;
		}

		case Value::STRING_VALUE: {
			std::string s;
			if (val.IsStringValue(s)) {
				if (stringBuffer) {
					auto [offset, size] = stringBuffer->addString(s);
					if (offset != 0 || size == 0) {  // Check for valid result
						outValue = InlineExprStorage::makeStringRef(offset, size);
						return true;
					}
				}
			}
			return false;
		}

		default:
			// Other value types (classad, list, etc.) cannot be inlined
			return false;
	}
}

ExprTree* inlineValueToExprTree(const InlineExprStorage& val, const InlineStringBuffer* stringBuffer) {
	if (!val.isInline()) {
		return val.asPtr();
	}

	// For inline values, we need to convert to Value first,
	// then create a Literal from that Value.
	// We use the global InlineStringBuffer which should be obtained
	// from the ClassAd that owns this value.
	Value v;
	int type = val.getInlineType();
	switch (type) {
		case 0:  // Error literal
			v.SetErrorValue();
			break;
		case 1:  // Undefined literal
			v.SetUndefinedValue();
			break;
		case 2: {  // int32
			v.SetIntegerValue(val.getInt32());
			break;
		}
		case 3: {  // float32
			v.SetRealValue(static_cast<double>(val.getFloat32()));
			break;
		}
		case 4: {  // bool
			v.SetBooleanValue(val.getBool());
			break;
		}
		case 5: {  // string - we can't convert without the buffer context
			uint32_t offset, size;
            val.getStringRef(offset, size);
            if (stringBuffer) {
                std::string s = stringBuffer->getString(offset, size);
                v.SetStringValue(s);
            } else {
                v.SetErrorValue();
            }
			break;
		}
		default:
			v.SetErrorValue();
			break;
	}
	return Literal::MakeLiteral(v);
}

bool inlineValueToValue(const InlineExprStorage& val, Value& outValue, const InlineStringBuffer* stringBuffer) {
	if (!val.isInline()) {
		// Not an inline value - caller should handle this differently
		return false;
	}

	int type = val.getInlineType();
	switch (type) {
		case 0:  // Error literal
			outValue.SetErrorValue();
			return true;
		case 1:  // Undefined literal
			outValue.SetUndefinedValue();
			return true;
		case 2: {  // int32
			outValue.SetIntegerValue(val.getInt32());
			return true;
		}
		case 3: {  // float32
			outValue.SetRealValue(static_cast<double>(val.getFloat32()));
			return true;
		}
		case 4: {  // bool
			outValue.SetBooleanValue(val.getBool());
			return true;
		}
		case 5: {  // string
			uint32_t offset, size;
			val.getStringRef(offset, size);
			if (stringBuffer) {
				std::string s = stringBuffer->getString(offset, size);
				outValue.SetStringValue(s);
				return true;
			}
			return false;
		}
		default:
			return false;
	}
}

bool InlineExprStorage::sameAs(const InlineExprStorage& other,
					  const InlineStringBuffer* selfBuffer,
					  const InlineStringBuffer* otherBuffer) const
{
	if (isInline() && other.isInline()) {
		return (toBits() == other.toBits());
	}

	if (!isInline() && !other.isInline()) {
		ExprTree* lhs = asPtr();
		ExprTree* rhs = other.asPtr();
		if (lhs == rhs) {
			return true;
		}
		if (!lhs || !rhs) {
			return false;
		}
		return lhs->SameAs(rhs);
	}

	const InlineExprStorage& inlineVal = isInline() ? *this : other;
	const InlineStringBuffer* inlineBuf = isInline() ? selfBuffer : otherBuffer;
	const InlineExprStorage& treeVal = isInline() ? other : *this;
	ExprTree* treePtr = treeVal.asPtr();

	if (!treePtr) {
		return false;
	}

	Value inlineValueData;
	if (!inlineValueToValue(inlineVal, inlineValueData, inlineBuf)) {
		return false;
	}

	const Literal* literalTree = dynamic_cast<const Literal*>(treePtr);
	if (literalTree) {
		Value literalValue;
		literalTree->GetValue(literalValue);
		return inlineValueData.SameAs(literalValue);
	}

	// Non-literal ExprTree cannot match an inline literal value; they are different types!
	return false;
}

} // namespace classad

namespace classad {

ExprTree* InlineExprStorage::asPtr() const {
	if (isInline()) {
		return nullptr;
	}
	return reinterpret_cast<ExprTree*>(_bits);
}

void InlineExprStorage::release() {
	if (!isInline()) {
		delete asPtr();
	}
	_bits = 0;
}

ExprTree* InlineExprStorage::materialize(InlineStringBuffer* stringBuffer) const {
	if (!isInline()) {
		return reinterpret_cast<ExprTree*>(_bits);
	}

	ExprTree* result = nullptr;
	if (getInlineType() == 5) {
		// Strings require the buffer to reconstruct the literal
		Value v;
		if (inlineValueToValue(*this, v, stringBuffer)) {
			result = Literal::MakeLiteral(v);
		}
	} else {
		result = inlineValueToExprTree(*this, stringBuffer);
	}

	_bits = reinterpret_cast<uint64_t>(result);
	return result;
}

bool InlineExprStorage::Evaluate(Value& val, const InlineStringBuffer* stringBuffer) const
{
	if (isInline()) {
		// Inline value - evaluate directly without materializing
		return inlineValueToValue(*this, val, stringBuffer);
	} else {
		// Out-of-line ExprTree* - delegate to its Evaluate method
		ExprTree* expr = asPtr();
		if (!expr) {
			val.SetErrorValue();
			return false;
		}
		return expr->Evaluate(val);
	}
}

} // namespace classad
