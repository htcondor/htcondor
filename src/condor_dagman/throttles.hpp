/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
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

#pragma once

#include <array>
#include <cstddef>

#include "condor_attributes.h"

enum class Throttle {
	MAX_IDLE = 0,             // Maximum number of jobs idle in the queue
	MAX_PRE,                  // Maximum number of executing pre scripts
	MAX_HOLD,                 // Maximum number of executing hold scripts
	MAX_POST,                 // Maximum number of executing post scripts
	MAX_NODES,                // Maximum number of executing nodes (i.e. jobs queued)
	MAX_INT_SUBMITS,          // Maximum number of job submission in an interval
	_SIZE // !!!!! Must be final item in enum !!!!!
};

// Keep inline with Throttle enum order
constexpr std::array<const char*, static_cast<size_t>(Throttle::_SIZE)> THROTTLE_DISPLAY = {{
	"Maximum Idle Jobs",
	"Maximum Pre Scripts",
	"Maximum Hold Scripts",
	"Maximum Post Scripts",
	"Maximum Submitted Nodes",
	"Maximum Submits Per Interval"
}};

// Keep inline with Throttle enum order
constexpr std::array<const char*, static_cast<size_t>(Throttle::_SIZE)> THROTTLE_ATTR = {{
	ATTR_DAGMAN_MAXIDLE,
	ATTR_DAGMAN_MAXPRESCRIPTS,
	ATTR_DAGMAN_MAXHOLDSCRIPTS,
	ATTR_DAGMAN_MAXPOSTSCRIPTS,
	ATTR_DAGMAN_MAXJOBS,
	ATTR_DAGMAN_MAXSUBMITSPERINT
}};

class Throttles {
public:
	Throttles() = default;

	bool WithinLimit(const Throttle t, int check) const {
		const int value = (*this)[t];
		return value <= 0 || check < value;
	}

	int operator[](Throttle t) const { return values[static_cast<size_t>(t)]; }
	int& operator[](Throttle t) { return values[static_cast<size_t>(t)]; }

	int operator[](size_t i) const { return values[i]; }
	int& operator[](size_t i) { return values[i]; }

	// Note: Use bitwise OR (|) operator to join two Throttles with the
	// this object taking precedence for setting the max/min of a value
	// otherwise use the right hand sides value
	// Note: Negative numbers and 0 constitute no limit
	Throttles operator|(const Throttles& rhs) const {
		Throttles result;

		for (size_t i = 0; i < static_cast<size_t>(Throttle::_SIZE); i++) {
			if (values[i] > 0 && (rhs[i] > values[i] || rhs[i] <= 0)) {
				result[i] = values[i];
			} else {
				result[i] = rhs[i];
			}

			// We accept negative numbers as infinity but set to zero for consistency
			if (result[i] < 0) { result[i] = 0; }
		}

		return result;
	}

	// Inherit any set throttle values (N >= 0) from right hand side (used for recovery)
	Throttles& operator^=(const Throttles& rhs) {
		for (size_t i = 0; i < static_cast<size_t>(Throttle::_SIZE); i++) {
			if (rhs[i] >= 0) {
				values[i] = rhs[i];
			}
		}

		return *this;
	}

private:
	// All values default to 0 which means no limit
	std::array<int, static_cast<size_t>(Throttle::_SIZE)> values{};
};
