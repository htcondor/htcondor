#pragma once

#include <array>

namespace LibrarianConfigOptions {
	enum class b { // Librarian boolean config options
		//Option = 0,
		_SIZE // MUST BE FINAL ITEM
	};

	enum class i { // Librarian integer config options
		DBMaxJobCacheSize = 0,                        // Maximum cache size for record JobId -> Job Table Unique Id
		UpdateInterval,                               // Interval in seconds the librarian should process archive files
		StatusRetentionSeconds,                       // How long (seconds) to retain Status rows in the DB
		_SIZE // MUST BE FINAL ITEM
	};

	enum class ll { // Librarian 64 byte integer config options
		DBMaxSizeBytes = 0,                           // Maximum size of data base in bytes
        MaxRecordsPerUpdate,                          // Maximum records to process per update
		_SIZE // MUST BE FINAL ITEM
	};

	enum class dbl { // Librarian double config options
		DBHighWaterMark = 0,                          // DB size percentage upper limit to trigger garbage collection
		DBLowWaterMark,                               // DB size percentage to reduce to upon garbage collection
		_SIZE // MUST BE FINAL ITEM
	};

	enum class str { // Librarian string config options
		DBPath = 0,                                    // Path to Data Base File
		ArchiveFile,                                   // Path to 'active' archive file to track
		_SIZE // MUST BE FINAL ITEM
	};
}

class LibrarianConfig {
public:
	LibrarianConfig() {
		using namespace LibrarianConfigOptions;

		intOpts[static_cast<size_t>(ll::MaxRecordsPerUpdate)] = 1'000'000;
		intOpts[static_cast<size_t>(i::DBMaxJobCacheSize)] = 10'000;
		intOpts[static_cast<size_t>(i::UpdateInterval)] = 30;
		intOpts[static_cast<size_t>(i::StatusRetentionSeconds)] = 300;
		int64Opts[static_cast<size_t>(ll::DBMaxSizeBytes)] = 2LL * 1024 * 1024 * 1024;
		doubleOpts[static_cast<size_t>(dbl::DBHighWaterMark)] = 0.97;
		doubleOpts[static_cast<size_t>(dbl::DBLowWaterMark)] = 0.80;
	}

	bool operator[](LibrarianConfigOptions::b opt) const { return boolOpts[static_cast<size_t>(opt)]; }
	bool& operator[](LibrarianConfigOptions::b opt) { return boolOpts[static_cast<size_t>(opt)]; }

	int operator[](LibrarianConfigOptions::i opt) const { return intOpts[static_cast<size_t>(opt)]; }
	int& operator[](LibrarianConfigOptions::i opt) { return intOpts[static_cast<size_t>(opt)]; }

	int64_t operator[](LibrarianConfigOptions::ll opt) const { return int64Opts[static_cast<size_t>(opt)]; }
	int64_t& operator[](LibrarianConfigOptions::ll opt) { return int64Opts[static_cast<size_t>(opt)]; }

	double operator[](LibrarianConfigOptions::dbl opt) const { return doubleOpts[static_cast<size_t>(opt)]; }
	double& operator[](LibrarianConfigOptions::dbl opt) { return doubleOpts[static_cast<size_t>(opt)]; }

	std::string& operator[](LibrarianConfigOptions::str opt) { return strOpts[static_cast<size_t>(opt)]; }
	const std::string& operator[](LibrarianConfigOptions::str opt) const { return strOpts[static_cast<size_t>(opt)]; }

private:
	std::array<bool, static_cast<size_t>(LibrarianConfigOptions::b::_SIZE)> boolOpts{};
	std::array<int, static_cast<size_t>(LibrarianConfigOptions::i::_SIZE)> intOpts{};
	std::array<int64_t, static_cast<size_t>(LibrarianConfigOptions::ll::_SIZE)> int64Opts{};
	std::array<double, static_cast<size_t>(LibrarianConfigOptions::dbl::_SIZE)> doubleOpts{};
	std::array<std::string, static_cast<size_t>(LibrarianConfigOptions::str::_SIZE)> strOpts{};
};

