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


#include "classad/exprTree.h"
#include "classad/value.h"
#include <vector>
#include <algorithm>
#include <string>
#include <string.h>
#include <memory>

namespace classad {

class ExprTree;
class InlineValue;  // Forward declaration
class InlineStringBuffer;  // Forward declaration

// Forward declaration from classad_inline_eval.h
bool unparseInlineValue(ExprTree* ptr, std::string& buffer,
                        const InlineStringBuffer* stringBuffer);

// ClassAdFlatMap
//
// This is the data structure that holds the map from std::string's to 
// ExprTree*'s.  
//
// Requirements:  We need to support hundreds of thousands of instances of
// these, and we rarely expect any indivdual map to grow to more than a few
// hundred entries.  We rarely insert into an existing one, compared to the
// number of lookups, updates, and scans/copies over an entire one.  Very
// frequently, one process will send an entire classad over the wire to another
// process, and we know at injestion time how large the classad is, and how
// much it is likely to grow, so we can perform a single allocation for the
// entire map.
//
// Therefore, we store the map as a std::vector of pairs of std::string and
// ExprTree *.  To speed lookup, we sort the strings, first by size (as that's
// fast to lookup), and then lexigraphically, insensitive to case.  If the
// sender and receiver have the same ordering function, the pairs will arrive
// in order, so the reciever will not need to shuffle them around in memory,
// further accelerating the process of injestion.
//
// Downsides: erasing or emplacing invaliate iterators, so it is UB (i.e.
// a crash) to insert or erase while iterating.


// The ordering function
struct ClassAdFlatMapOrder {
	ClassAdFlatMapOrder()  = default;

	bool operator()(const std::pair<std::string, InlineValue> &lhs, const std::string &rhs) noexcept {
		if (lhs.first.size() < rhs.size()) return true;
		if (lhs.first.size() > rhs.size()) return false;
		return strcasecmp(lhs.first.c_str(), rhs.c_str()) < 0;	
	}

	bool operator()(const std::pair<std::string, InlineValue> &lhs, const char *rhs) noexcept {
		size_t len = strlen(rhs);
		if (lhs.first.size() < len) return true;
		if (lhs.first.size() > len) return false;
		return strcasecmp(lhs.first.c_str(), rhs) < 0;	
	}

	bool operator()(const std::string &lhs, const std::string &rhs) noexcept {
		if (lhs.size() < rhs.size()) return true;
		if (lhs.size() > rhs.size()) return false;
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;	
	}
};

// note this needs to be inline for ODR reasons
inline bool ClassAdFlatMapEqual(const std::pair<std::string, InlineValue>&lhs, const std::string &rhs) {
	return 0 == strcasecmp(lhs.first.c_str(), rhs.c_str());
}

inline bool ClassAdFlatMapEqual(const std::pair<std::string, InlineValue>&lhs, const char *rhs) {
	return 0 == strcasecmp(lhs.first.c_str(), rhs);
}


class ClassAdFlatMap {
	public:
		// Rule of zero for ctors/dtors/assignment/move
	
		using keyValue = std::pair<std::string, InlineValue>;
		using container = std::vector<keyValue>;
		using iterator = container::iterator;
		using const_iterator = container::const_iterator;

		ClassAdFlatMap() = default;

		// The iterators
		iterator begin() { return _theVector.begin(); }
		const_iterator begin() const { return _theVector.cbegin(); }
		iterator end() { return _theVector.end(); }
		const_iterator end() const { return _theVector.cend(); }

		size_t size() const { return _theVector.size();}
		bool empty() const { return _theVector.empty();}

		// This is named rehash for comptibility with the earlier hashtable based implementation
		void rehash(size_t capacity) { _theVector.reserve(capacity); return;}

		void clear() { 
			for (auto &it: _theVector) {
				it.second.release();
			}
			_theVector.clear();
			if (_stringBuffer) {
				_stringBuffer->clear();
			}
		}

		// Ensure the string buffer exists (inline string storage helper)
		InlineStringBuffer* ensureStringBuffer() {
			if (!_stringBuffer) {
				_stringBuffer = std::make_unique<InlineStringBuffer>();
			}
			return _stringBuffer.get();
		}

		// Materialize helpers that hide buffer details
		ExprTree* materialize(const InlineValue& val) const {
			return const_cast<InlineValue&>(val).materialize(_stringBuffer.get());
		}

		ExprTree* materialize(const iterator& it) {
			return it->second.materialize(_stringBuffer.get());
		}

		ExprTree* materialize(const const_iterator& it) const {
			return const_cast<InlineValue&>(it->second).materialize(_stringBuffer.get());
		}

		bool inlineValueToValue(const InlineValue& val, Value& outVal) const {
			return classad::inlineValueToValue(val, outVal, _stringBuffer.get());
		}

		bool unparseInline(ExprTree* ptr, std::string& buffer) const {
			return classad::unparseInlineValue(ptr, buffer, _stringBuffer.get());
		}

		const InlineStringBuffer* stringBuffer() const {
			return _stringBuffer.get();
		}

		template <typename StringLike>
		iterator find(const StringLike &key) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				return lb;
			} else  {
				return end();
			}
		}

		template <typename StringLike>
		const_iterator find(const StringLike &key) const {
			const_iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				return lb;
			} else  {
				return end();
			}
		}

		// This is the hack for compat with clients who expect the hash interface
		// Ideally should deprecate this in the future
		// Returns reference to the ExprTree* inside the InlineValue, materializing inline values if necessary
		ExprTree *&  operator[](const std::string &key) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.materialize(_stringBuffer.get());
				return *reinterpret_cast<ExprTree**>(&lb->second._bits);
			} else {
				iterator newit = _theVector.insert(lb, std::make_pair(key, InlineValue(nullptr)));
				return *reinterpret_cast<ExprTree**>(&newit->second._bits);
			}
		}

		// This allows clients to safely erase as they iterate
		iterator erase(const iterator &it) { 
			it->second.release();
			return _theVector.erase(it);
		}

		template <typename StringLike>
		iterator erase(const StringLike &key) { 
			iterator it = find(key);
			if (it != _theVector.end()) {
				it->second.release();
				return _theVector.erase(it);
			}
			return _theVector.end();
		}

		// Store inline values directly - these create InlineValue internally and move into storage
		void setBool(const std::string &key, bool value) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.release();
				lb->second = InlineValue::makeBool(value);
			} else {
				_theVector.insert(lb, std::make_pair(key, InlineValue::makeBool(value)));
			}
		}

		void setInt(const std::string &key, int32_t value) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.release();
				lb->second = InlineValue::makeInt32(value);
			} else {
				_theVector.insert(lb, std::make_pair(key, InlineValue::makeInt32(value)));
			}
		}

		void setFloat(const std::string &key, float value) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.release();
				lb->second = InlineValue::makeFloat32(value);
			} else {
				_theVector.insert(lb, std::make_pair(key, InlineValue::makeFloat32(value)));
			}
		}

		void setString(const std::string &key, const std::string &value) {
			ensureStringBuffer();
			auto [offset, size] = _stringBuffer->addString(value);
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.release();
				lb->second = InlineValue::makeStringRef(offset, size);
			} else {
				_theVector.insert(lb, std::make_pair(key, InlineValue::makeStringRef(offset, size)));
			}
		}

		void setExpr(const std::string &key, ExprTree* expr) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.release();
				lb->second = InlineValue(expr);
			} else {
				_theVector.insert(lb, std::make_pair(key, InlineValue(expr)));
			}
		}

		void setUndefined(const std::string &key) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.release();
				lb->second = InlineValue::makeUndefinedLiteral();
			} else {
				_theVector.insert(lb, std::make_pair(key, InlineValue::makeUndefinedLiteral()));
			}
		}

		void setError(const std::string &key) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				lb->second.release();
				lb->second = InlineValue::makeErrorLiteral();
			} else {
				_theVector.insert(lb, std::make_pair(key, InlineValue::makeErrorLiteral()));
			}
		}

		template <typename StringLike> 
		std::pair<iterator, bool> emplace(const StringLike &key, ExprTree *value) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder{});

			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				return std::make_pair(lb, false);
			} else {
				iterator newit = _theVector.insert(lb, std::make_pair(key, InlineValue(value)));
				return std::make_pair(newit, true);
			}
		}

	private:
		container _theVector;
		std::unique_ptr<InlineStringBuffer> _stringBuffer;
};
}
