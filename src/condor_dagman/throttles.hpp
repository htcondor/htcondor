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

#include <map>

enum class Throttle {
	MAX_IDLE = 0,             // Maximum number of jobs idle in the queue
	MAX_PRE,                  // Maximum number of executing pre scripts
	MAX_HOLD,                 // Maximum number of executing hold scripts
	MAX_POST,                 // Maximum number of executing post scripts
	MAX_NODES,                // Maximum number of executing nodes (i.e. jobs queued)
	MAX_INT_SUBMITS,          // Maximum number of job submission in an interval
	_SIZE // !!!!! Must be final item in enum !!!!!
};

const std::map<Throttle, const char*> ThrottleType = {
	{Throttle::MAX_IDLE , "Maximum Idle Jobs"},
	{Throttle::MAX_PRE , "Maximum Pre Scripts"},
	{Throttle::MAX_HOLD , "Maximum Hold Scripts"},
	{Throttle::MAX_POST , "Maximum Post Scripts"},
	{Throttle::MAX_NODES , "Maximum Submitted Nodes"},
	{Throttle::MAX_INT_SUBMITS , "Maximum Submits Per Interval"}
};


class Throttles {
public:
	Throttles() = default;

	bool WithinLimit(const Throttle t, int check) const {
		const int value = (*this)[t];
		return value <= 0 || check < value;
	}

	bool operator==(const Throttles& rhs) const {
		Throttles self = *this;

		for (size_t i = 0; i < static_cast<size_t>(Throttle::_SIZE); i++) {
			const Throttle t = static_cast<Throttle>(i);
			if (self[t] != rhs[t]) {
				return false;
			}
		}

		return true;
	}

	int operator[](Throttle t) const { return values[static_cast<size_t>(t)]; }
	int& operator[](Throttle t) { return values[static_cast<size_t>(t)]; }

	// Note: Use bitwise operator to join two Throttles with the
	// this object taking precedence for setting the max/min of a value
	// otherwise use the right hand sides value
	// Note: Negative numbers and 0 constitue no limit
	Throttles operator|(const Throttles& rhs) const {
		Throttles result;
		Throttles self = *this;

		for (size_t i = 0; i < static_cast<size_t>(Throttle::_SIZE); i++) {
			const Throttle t = static_cast<Throttle>(i);
			if (self[t] > 0 && (rhs[t] > self[t] || rhs[t] <= 0)) {
				result[t] = self[t];
			} else {
				result[t] = rhs[t];
			}
		}

		return result;
	}

private:
	// All values default to 0 which means no limit
	std::array<int, static_cast<size_t>(Throttle::_SIZE)> values;
};
