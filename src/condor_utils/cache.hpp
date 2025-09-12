#ifndef CACHE_HPP
#define CACHE_HPP

#include <map>
#include <vector>
#include <ctime>
#include <stdio.h>

template<typename K, typename V>
class Cache {
public:
	Cache() = default;
	Cache(size_t limit) : _limit(limit) {};

	// Manually set (or reset) cache limit
	void setLimit(size_t limit) { _limit = limit; }

	// Insert a key/value pair. Replace oldest entry if at limit
	void insert(const K& key, const V& value) {
		_hash[key] = std::make_pair(value, time(nullptr));
		checkLimit();
	}

	// Check if cache is empty
	bool empty() const { return _hash.empty(); }
	// Check if key exists in cache
	bool contains(const K& key) const { return _hash.contains(key); }
	// Get current cache size
	size_t size() const { return _hash.size(); }
	// Get current cache limit (max size)
	size_t limit() const { return _limit; }

	// Access cached information for provided key
	// Note: If key DNE then entry is added and limit checking is applied
	//       Use contains function to check if key/value is cached already
	V& operator[](const K& key) { return get(key); }
	const V& operator[](const K& key) const { return get(key); }

	// Remove cached key/value if it exists
	void remove(const K& key) { if (_hash.contains(key)) _hash.erase(key); }
	// Completely empty cache
	void clear() { _hash.clear(); }
	// Empty up to N oldest cache entries
	void clear(size_t threshold) { removeLRU(threshold); }

	const std::map<K, std::pair<V, time_t>>& getHash() const { return _hash; }

private:
	// Check cache is beyond limit and remove up to N oldest entries
	void checkLimit(size_t threshold = 1) {
		if (_limit && _hash.size() > _limit) {
			removeLRU(threshold);
		}
	}

	// Get cached data for provided key and update access time
	V& get(const K& key) {
		auto& [value, last_access] = _hash[key];
		last_access = time(nullptr);
		checkLimit();
		return value;
	}

	// Remove up to N oldest etries from cache
	void removeLRU(size_t threshold) {
		if (threshold <= 0) { return; }

		std::vector<std::pair<K, time_t>> check;
		for (const auto& [key, details] : _hash) {
			auto& [_, last_access] = details;
			check.emplace_back(key, last_access);
		}

		std::ranges::stable_sort(check, std::ranges::less(), &std::pair<K, time_t>::second);

		size_t i = 0;
		for (const auto& [key, _] : check) {
			if (i++ >= threshold) { break; }
			_hash.erase(key);
		}
	}

	std::map<K, std::pair<V, time_t>> _hash{}; // Actual map to act as cache {key : pair{value, access time}}
	size_t _limit{0}; // Zero is no limit
};

#endif /* CACHE_HPP */
