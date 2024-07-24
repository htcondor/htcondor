/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
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

#ifndef __FLAT_SET_H__
#define __FLAT_SET_H__

#include <vector>

// flat_set
//
// This is a replacement for std::set that uses a sorted std::vector to
// store the elements of the set. This improves memory usage (one big
// allocation instead of many small allocations and less total memory used)
// at the expense of making insert and erase more computationally expensive
// (many elements may have to be moved in memory for each operation).
// In many situations, the trade-off is worthwhile.
//
// This class is meant to be a drop-in replacement for std::set within the
// HTCondor codebase. It doesn't replicate the entrie API of std::set, but
// the missing parts can be filled in as needed.
//
// NOTE: The insert(), emplace(), and erase() operations can invalidate all
// existing iterators, which is different than std::set. When performing
// these operations while iterating, only the iterator returned by the
// method will be valid after each call.

template <class T, class Compare = std::less<T> >
class flat_set {
 public:
	flat_set(const Compare& c = Compare())
	: _cmp(c) {}

	template< class InputIt >
	flat_set(InputIt first, InputIt last, const Compare& c = Compare())
	: _vec(first, last), _cmp(c)
	{ std::sort(_vec.begin(), _vec.end(), _cmp); }

	flat_set(std::initializer_list<T> init, const Compare& c = Compare())
	:  _vec(init), _cmp(c)
	{ std::sort(_vec.begin(), _vec.end(), _cmp); }

	using iterator = std::vector<T>::iterator;
	using const_iterator = std::vector<T>::const_iterator;

	// The iterators
	iterator begin() { return _vec.begin(); }
	const_iterator begin() const { return _vec.cbegin(); }
	iterator end() { return _vec.end(); }
	const_iterator end() const { return _vec.cend(); }

	size_t size() const { return _vec.size();}
	bool empty() { return _vec.empty(); }

	void clear() { _vec.clear(); }
	void reserve(size_t sz) { _vec.reserve(sz); }

	iterator find(const T& val) {
		iterator lb = std::lower_bound(begin(), end(), val, _cmp);
		if (lb == end() || _cmp(val, *lb)) {
			return end();
		} else  {
			return lb;
		}
	}

	const_iterator find(const T& val) const {
		const_iterator lb = std::lower_bound(begin(), end(), val, _cmp);
		if (lb == end() || _cmp(val, *lb)) {
			return end();
		} else  {
			return lb;
		}
	}

	size_t count(const T& val) const {
		return (find(val) == end()) ? 0 : 1;
	}

	bool contains(const T& val) const {
		return (find(val) != end());
	}

	// This allows clients to safely erase as they iterate
	iterator erase(const iterator &it) {
		return _vec.erase(it);
	}

	size_t erase(const T& val) {
		iterator it = find(val);
		if (it != _vec.end()) {
			_vec.erase(it);
			return 1;
		}
		return 0;
	}

	std::pair<iterator, bool> insert(const T& val) {
		iterator lb = std::lower_bound(begin(), end(), val, _cmp);

		if (lb == end() || _cmp(val, *lb)) {
			iterator newit = _vec.insert(lb, val);
			return std::make_pair(newit, true);
		} else {
			return std::make_pair(lb, false);
		}
	}

	template< class InputIt >
	void insert( InputIt first, InputIt last ) {
		while (first != last) {
			insert(*(first++));
		}
	}

	template< class... Args >
	std::pair<iterator, bool> emplace(Args&&... args) {
		T val(std::forward<Args>(args)...);
		iterator lb = std::lower_bound(begin(), end(), val, _cmp);

		if (lb == end() || _cmp(val, *lb)) {
			iterator newit = _vec.insert(lb, std::move(val));
			return std::make_pair(newit, true);
		} else {
			return std::make_pair(lb, false);
		}
	}

	friend bool operator==(const flat_set<T, Compare>& lhs, const flat_set<T, Compare>& rhs)
	{ return operator==(lhs._vec, rhs._vec); }

	friend bool operator<=>(const flat_set<T, Compare>& lhs, const flat_set<T, Compare>& rhs)
	{ return operator<=>(lhs._vec, rhs._vec); }


 private:
	std::vector<T> _vec;
	Compare _cmp;
};

#endif // __FLAT_SET_H__
