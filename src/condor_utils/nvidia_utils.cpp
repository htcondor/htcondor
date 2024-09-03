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
#ifdef LINUX

#include "condor_common.h"
#include <tuple>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <charconv>
#include <stdio.h>
#include <cstring>

#include "stl_string_utils.h"
#include "condor_debug.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

using uuidMap = std::vector<std::string, dev_t>;

// Return a single std::string with the contents of a file path
// or empty string if can't read the file

// Note that we can't use readShortFile because these files in 
// /proc and /dev report a 0 file size length when stat(2)'ed, which
// those utilities rely on.  Here we just read until EOF.

static std::string
file_to_string(const std::filesystem::path &p) {
	std::string contents;
	FILE *f = fopen(p.c_str(), "r");
	if (f) {
		char buf[128];
		while (fgets(buf, sizeof(buf) - 1,f)) {
			contents += buf;
		}
		fclose(f);
	} 
	return contents;
}

// Return the linux major device num for all nvidia devices
// or 0 if no accessible nvidia on system (0 is an invalid device num)
static uint64_t
nvidia_major_device() {
	uint64_t major = 0;

	struct stat statbuf {};
	int r = stat("/dev/nvidiactl", &statbuf);
	if (r == 0) {
		major = major(statbuf.st_rdev);
	}
	return major;
}

// Given the entire contents of /proc/driver/nvidia/gpus/<gpu bus id>/information
// return the gpu UUID string and the device minor number parsed from that info
static std::pair<std::string, uint64_t>
string_to_nvidia_uuid_and_minor(const std::string &gpu_info) {
	std::string uuid;
	uint64_t minor = 255;

	// Parse line like
	// GPU UUID: \t GPU-xxxx-xxx-xxx\n

	size_t before_uuid_pos = gpu_info.find("\nGPU UUID: ");
	size_t uuid_pos        = gpu_info.find_first_not_of(" \t", strlen("\nGPU UUID: ") + before_uuid_pos);
	size_t uuid_pos_end    = gpu_info.find_first_of('\n', uuid_pos);
	if (before_uuid_pos != std::string::npos) {
		uuid = gpu_info.substr(uuid_pos, uuid_pos_end - uuid_pos);
	}

	// Parse line like
	// Device Minor: \t 7\n
	size_t before_minor_pos = gpu_info.find("\nDevice Minor: ");
	size_t minor_pos        = gpu_info.find_first_not_of(" \t", strlen("\nDevice Minor: ") + before_minor_pos);
	size_t minor_pos_end    = gpu_info.find_first_of('\n', minor_pos);

	if (before_minor_pos != std::string::npos) {
		std::string minorstr = gpu_info.substr(minor_pos, minor_pos_end - minor_pos);
		std::from_chars(minorstr.data(), minorstr.data() + minorstr.size(), minor);
	}
	return std::make_pair(uuid, minor);
}

// Parse all of the /proc/driver/nvidia/gpus/*/information files
// to get the uuid and device minor ids for every nvidia gpu
// in the system, and return a vector of pairs of uuid to device
// for later inspection.
//
// If no GPUs on the system, or they are inaccesible, return empty
// vector.
std::vector<std::pair<std::string, dev_t>>
make_nvidia_uuid_to_dev_map() {
	std::vector<std::pair<std::string, dev_t>> uuid_map;

	const std::filesystem::path gpu_root { "/proc/driver/nvidia/gpus" };
	uint64_t major = nvidia_major_device();
	if (major == 0) {
		// no device, return empty
		return uuid_map;
	}

	std::error_code ec;
	for (const auto &pci_path : std::filesystem::directory_iterator(gpu_root,ec)) {
		if (pci_path.is_directory()) {
			std::filesystem::path info_path = gpu_root / pci_path / "information";
			std::string gpu_info = file_to_string(info_path);

			auto [uuid, minor] = string_to_nvidia_uuid_and_minor(gpu_info);
			uuid_map.emplace_back(uuid, makedev(major, minor));
		}
	}
	return uuid_map;
}

// public function to take a environment variable like defined in 
// CUDA_VISIBLE_DEVICES, and turn it into a vector of device major/minor
// to exclude.
std::vector<dev_t>
nvidia_env_var_to_exclude_list(const std::string &visible_devices_env) {
	std::vector<dev_t> exclude;

	std::string visible_str = visible_devices_env;
	trim(visible_str);

	// "all" means exclude no devices
	if (visible_str == "all") {
		return exclude;
	}

	// Map of nvidia gpu UUIDs to device major/minor
	auto uuid_map = make_nvidia_uuid_to_dev_map();

	// Remove from the map all uuids in our visible device string list
	for (const auto &uuid: StringTokenIterator(visible_str)) {
		size_t count = std::erase_if(uuid_map, [&uuid](const auto &p) {return p.first == uuid;});
		if (count == 0) {
			// Sanity checking.
			dprintf(D_ALWAYS, "Unknown GPU %s in NVIDIA_VISIBLE_DEVICES, skipping device hiding\n", uuid.c_str());
			return {};
		} 
	}

	// And copy all remaining visible devices to our map
	for (const auto &[uuid, dev]: uuid_map) {
		exclude.emplace_back(dev);
	}

	return exclude;
}

#endif
