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

#ifndef __CLASSAD_INLINE_VALUE_H__
#define __CLASSAD_INLINE_VALUE_H__

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include <memory>

namespace classad {

class ExprTree;
class Value;
class InlineStringBuffer;
class ClassAdFlatMap;

// Inline value encoding strategy:
// We use the LSB (least significant bit) of the ExprTree* pointer to indicate whether
// the value is inline or a pointer to an ExprTree.
// 
// If LSB == 0: Standard ExprTree* pointer
// If LSB == 1: Inline encoded value with format:
//   Byte 0 (bits 7-1): Type code
//   Byte 1-7: Type-specific data
//
// Type codes:
//   0 = Error literal
//   1 = Undefined literal
//   2 = int32 value
//   3 = float32 value
//   4 = bool value
//   5 = String (offset and size into global string buffer)
//   6-7 = Reserved
//
// Note: On little-endian systems, the LSB is at address pointer & 1
// This requires that ExprTree objects are always allocated on 2-byte boundaries,
// which is guaranteed by the allocator since ExprTree has alignment > 1.

class InlineValue {
public:
	InlineValue() : _bits(0) {}
	explicit InlineValue(ExprTree* p) : _bits(reinterpret_cast<uint64_t>(p)) {}
	explicit InlineValue(uint64_t b) : _bits(b) {}

	// Delete copy constructor and assignment - InlineValue is not copyable
	// because inline strings refer to an external ClassAd-specific string buffer
	InlineValue(const InlineValue&) = delete;
	InlineValue& operator=(const InlineValue&) = delete;

	// Allow move semantics for returning from factory functions
	InlineValue(InlineValue&& other) noexcept : _bits(other._bits) {
		other._bits = 0;
	}
	InlineValue& operator=(InlineValue&& other) noexcept {
		if (this != &other) {
			_bits = other._bits;
			other._bits = 0;
		}
		return *this;
	}

	// Check if this is an inline value
	bool isInline() const { return (_bits & 1u) != 0; }

	// Boolean operator - returns false if bits are all zero (equivalent to null pointer)
	explicit operator bool() const { return _bits != 0; }

	// Update this InlineValue to point to a different ExprTree pointer
	void updatePtr(ExprTree* p) { _bits = reinterpret_cast<uint64_t>(p); }
	// Get the type of an inline value (assumes isInline() == true)
	int getInlineType() const { return static_cast<int>((_bits >> 1) & 0x7); }

	// Get the data portion of an inline value
	uint64_t getInlineData() const { return (_bits >> 8); }

	// Create an inline error literal
    static InlineValue makeErrorLiteral() {
        InlineValue v;
        v._bits = 1 | (0 << 1);  // Set LSB and type 0
        return v;
    }

	// Create an inline undefined literal
    static InlineValue makeUndefinedLiteral() {
        InlineValue v;
        v._bits = 1 | (1 << 1);  // Set LSB and type 1
        return v;
    }

	// Create an inline int32 value
    static InlineValue makeInt32(int32_t value) {
        InlineValue v;
        uint64_t val = static_cast<uint64_t>(value) & 0xFFFFFFFF;
        v._bits = 1 | (2 << 1) | (val << 8);
        return v;
    }

	// Create an inline float32 value
    static InlineValue makeFloat32(float value) {
        InlineValue v;
        uint32_t fval;
        std::memcpy(&fval, &value, sizeof(float));
        v._bits = 1 | (3 << 1) | (static_cast<uint64_t>(fval) << 8);
        return v;
    }

	// Create an inline bool value
    static InlineValue makeBool(bool value) {
        InlineValue v;
        v._bits = 1 | (4 << 1) | (static_cast<uint64_t>(value ? 1 : 0) << 8);
        return v;
    }

	// Create an inline string reference (offset and size in global buffer)
	// offset and size must each fit in 24 bits
    static InlineValue makeStringRef(uint32_t offset, uint32_t size) {
        InlineValue v;
        // Offset in bits 8-31 (24 bits), size in bits 32-55 (24 bits)
        uint64_t data = (static_cast<uint64_t>(offset) & 0xFFFFFF) |
                        ((static_cast<uint64_t>(size) & 0xFFFFFF) << 24);
        v._bits = 1 | (5 << 1) | (data << 8);
        return v;
    }

	// Extract int32 from inline value (assumes type 2)
    int32_t getInt32() const { return static_cast<int32_t>((_bits >> 8) & 0xFFFFFFFF); }

	// Extract float32 from inline value (assumes type 3)
    float getFloat32() const {
        uint32_t fval = static_cast<uint32_t>((_bits >> 8) & 0xFFFFFFFF);
        float result;
        std::memcpy(&result, &fval, sizeof(float));
        return result;
    }

	// Extract bool from inline value (assumes type 4)
    bool getBool() const { return ((_bits >> 8) & 1u) != 0; }

	// Extract string offset and size from inline value (assumes type 5)
    void getStringRef(uint32_t& offset, uint32_t& size) const {
        uint64_t data = _bits >> 8;
        offset = data & 0xFFFFFF;
        size = (data >> 24) & 0xFFFFFF;
    }

	// Cast to ExprTree* (only valid if !isInline())
    ExprTree* asPtr() const;

	// Return or create an ExprTree*; if inline, materialize and cache as out-of-line.
	ExprTree* materialize(InlineStringBuffer* stringBuffer) const;

	// Create a copy of this InlineValue.
	// If inline: returns a copy of the inline bits (cheap, no allocation)
	// If out-of-line: calls Copy() on the ExprTree and returns new InlineValue wrapping it
	InlineValue Copy() const;

	// Delete owned pointer if out-of-line and clear the bits
	void release();

	// Compare two inline/out-of-line values without forcing materialization
	bool sameAs(const InlineValue& other,
	           const InlineStringBuffer* selfBuffer,
	           const InlineStringBuffer* otherBuffer) const;

	// Evaluate inline value to a Value without materializing if possible
	// Returns true if evaluation succeeded, false otherwise
	bool Evaluate(Value& val, const InlineStringBuffer* stringBuffer = nullptr) const;

	// Access raw bits (for internal/friend use only)
	uint64_t toBits() const { return _bits; }
	void setBits(uint64_t b) { _bits = b; }

	// Get a mutable reference to the pointer (only valid when !isInline())
	// This provides a safe way to return ExprTree*& without violating strict-aliasing
	// After materialize() is called, _bits contains the pointer value directly
	ExprTree*& asPtrRef() {
		// At this point, materialize() has been called, so _bits contains a pointer
		// We use a union to safely access the bits as a pointer
		union {
			uint64_t* bits_ptr;
			ExprTree** expr_ptr;
		} u;
		u.bits_ptr = &_bits;
		return *u.expr_ptr;
	}

private:
	mutable uint64_t _bits;

	friend bool tryMakeInlineValue(const Value& val, InlineValue& outValue, InlineStringBuffer* stringBuffer);
	friend ExprTree* inlineValueToExprTree(const InlineValue& val, const InlineStringBuffer* stringBuffer);
	friend bool inlineValueToValue(const InlineValue& val, Value& outValue, const InlineStringBuffer* stringBuffer);
	friend class ClassAdFlatMap;
	friend class ClassAd;
};

/**
 * InlineExpr bundles an InlineValue with its owning AttrList (ClassAdFlatMap).
 * This prevents accidentally using an InlineValue with the wrong buffer, which
 * is critical for inline string values that reference buffer-specific offsets.
 */
class InlineExpr {
public:
	// Default constructor creates an empty InlineExpr
	InlineExpr() : _value(nullptr), _attrList(nullptr) {}

	// Check if this represents a valid expression
	explicit operator bool() const { return _value && *_value; }

	// Check if the value is inline or out-of-line
	bool isInline() const { return _value && _value->isInline(); }

	// Get the InlineValue (for internal use)
	const InlineValue* value() const { return _value; }

	// Get the AttrList (for internal use)
	const ClassAdFlatMap* attrList() const { return _attrList; }

	// Materialize to ExprTree* (handles both inline and out-of-line)
	ExprTree* materialize() const;

private:
	// Private constructor - use ClassAd::LookupInline() to obtain InlineExpr objects
	InlineExpr(const InlineValue& val, const ClassAdFlatMap* map)
		: _value(&val), _attrList(map) {}

	friend class ClassAd;  // Needs to construct InlineExpr in LookupInline()
	friend class ClassAdIterator;  // Needs to construct InlineExpr in operator*()
	friend class ClassAdConstIterator;  // Needs to construct InlineExpr in operator*()

	const InlineValue* _value;
	const ClassAdFlatMap* _attrList;
};

// Helper class to manage a global string buffer for ClassAdFlatMap
class InlineStringBuffer {
public:
	InlineStringBuffer() = default;
	~InlineStringBuffer() = default;

	// Add a string to the buffer, return offset and size
	// Returns {0, 0} if the string doesn't fit
	std::pair<uint32_t, uint32_t> addString(const std::string& str) {
		// Check if string fits in 24-bit size (16MB max)
		if (str.size() > 0xFFFFFF) {
			return {0, 0};
		}

		uint32_t offset = static_cast<uint32_t>(_buffer.size());
		
		// Check if offset would overflow 24 bits
		if (offset > 0xFFFFFF) {
			return {0, 0};
		}

		_buffer.insert(_buffer.end(), str.begin(), str.end());
		return {offset, static_cast<uint32_t>(str.size())};
	}

	// Get a string from the buffer
	std::string getString(uint32_t offset, uint32_t size) const {
		if (offset + size > _buffer.size()) {
			return "";
		}
		return std::string(_buffer.begin() + offset, _buffer.begin() + offset + size);
	}

	// Clear the buffer
	void clear() {
		_buffer.clear();
	}

	// Get the total size of the buffer
	size_t size() const {
		return _buffer.size();
	}

private:
	std::vector<char> _buffer;
};

// Try to convert a Value to an inline representation
// Returns true if successful, false if the value must remain as an ExprTree*
bool tryMakeInlineValue(const Value& val, InlineValue& outValue, InlineStringBuffer* stringBuffer);

// Convert an inline value to an ExprTree*
ExprTree* inlineValueToExprTree(const InlineValue& val, const InlineStringBuffer* stringBuffer);

// Convert an inline value to a Value object
bool inlineValueToValue(const InlineValue& val, Value& outValue, const InlineStringBuffer* stringBuffer);

} // namespace classad

#endif // __CLASSAD_INLINE_VALUE_H__
