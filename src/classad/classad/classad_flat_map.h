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
#include <vector>
#include <algorithm>
#include <string>
#include <string.h>

namespace classad {

class ExprTree;

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
	const size_t len = 0;
	ClassAdFlatMapOrder(const std::string &s) : len(s.size()) {}
	ClassAdFlatMapOrder(const char *s) : len(strlen(s)) {}
	ClassAdFlatMapOrder()  = default;

	bool operator()(const std::pair<std::string, ExprTree *> &lhs, const std::string &rhs) noexcept {
		if (lhs.first.size() < this->len) return true;
		if (lhs.first.size() > this->len) return false;
		return strcasecmp(lhs.first.c_str(), rhs.c_str()) < 0;	
	}

	bool operator()(const std::pair<std::string, ExprTree *> &lhs, const char *rhs) noexcept {
		if (lhs.first.size() < this->len) return true;
		if (lhs.first.size() > this->len) return false;
		return strcasecmp(lhs.first.c_str(), rhs) < 0;	
	}

	bool operator()(const std::string &lhs, const std::string &rhs) noexcept {
		if (lhs.size() < rhs.size()) return true;
		if (lhs.size() > rhs.size()) return false;
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;	
	}
};

// note this needs to be inline for ODR reasons
inline bool ClassAdFlatMapEqual(const std::pair<std::string, ExprTree *>&lhs, const std::string &rhs) {
	return 0 == strcasecmp(lhs.first.c_str(), rhs.c_str());
}

inline bool ClassAdFlatMapEqual(const std::pair<std::string, ExprTree *>&lhs, const char *rhs) {
	return 0 == strcasecmp(lhs.first.c_str(), rhs);
}


class ClassAdFlatMap {
	public:
		// Rule of zero for ctors/dtors/assignment/move
	
		using keyValue = std::pair<std::string, ExprTree *>;
		using container = std::vector<keyValue>;
		using iterator = container::iterator;
		using const_iterator = container::const_iterator;

		// The iterators
		iterator begin() { return _theVector.begin(); }
		const_iterator begin() const { return _theVector.cbegin(); }
		iterator end() { return _theVector.end(); }
		const_iterator end() const { return _theVector.cend(); }

		size_t size() const { return _theVector.size();}

		// This is named rehash for comptibility with the earlier hashtable based implementation
		void rehash(size_t capacity) { _theVector.reserve(capacity); return;}

		void clear() { 
			for (auto &it: _theVector) {
				delete it.second;
			}
			_theVector.clear();
		}

		template <typename StringLike>
		iterator find(const StringLike &key) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder(key));
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				return lb;
			} else  {
				return end();
			}
		}

		template <typename StringLike>
		const_iterator find(const StringLike &key) const {
			const_iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder(key));
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				return lb;
			} else  {
				return end();
			}
		}

		// This is the hack for compat with clients who expect the hash interface
		// Ideally should deprecate this in the future
		ExprTree *&  operator[](const std::string &key) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder(key));
			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				return lb->second;
			} else {
				return _theVector.insert(lb, std::make_pair(key, nullptr))->second;
			}
		}

		// This allows clients to safely erase as they iterate
		iterator erase(const iterator &it) { 
			delete it->second;
			return _theVector.erase(it);
		}

		template <typename StringLike>
		iterator erase(const StringLike &key) { 
			iterator it = find(key);
			if (it != _theVector.end()) {
				delete it->second;
				return _theVector.erase(it);
			}
			return _theVector.end();
		}

		template <typename StringLike> 
		std::pair<iterator, bool> emplace(const StringLike &key, ExprTree *value) {
			iterator lb = std::lower_bound(begin(), end(), key, ClassAdFlatMapOrder(key));

			if (lb != end() && ClassAdFlatMapEqual(*lb, key)) {
				return std::make_pair(lb, false);
			} else {
				iterator newit = _theVector.insert(lb, std::make_pair(key, value));
				return std::make_pair(newit, true);
			}
		}

	private:
		container _theVector;
};
}
