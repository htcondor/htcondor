/***************************************************************
 *
 * Copyright (C) 2026, Center for High Throughput Computing
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

#ifndef PLACEMENTD_EDIT_H
#define PLACEMENTD_EDIT_H

// PLACEMENT_EDIT: an atomic batch of Create, Update, Delete and Adopt
// operations on internal rows.

#include "placementd_db.h"
#include "placementd_reconcile.h"

// Upper bound on NumOperations.
constexpr int PLACEMENTD_MAX_EDIT_OPS = 1000;

// Caps on the request header's free text, after control characters are
// stripped.
constexpr size_t PLACEMENTD_MAX_ON_BEHALF_OF = 256;
constexpr size_t PLACEMENTD_MAX_COMMENT = 4096;

struct PlacementEditContext
{
	std::string peer;					// FQU of the CEDAR peer; the change set's actor
	std::optional<std::string> peer_address;
	// From the request header, already sanitized; recorded in the change
	// set, never used to authorize.
	std::optional<std::string> on_behalf_of;
	std::optional<std::string> comment;
	std::string uid_domain;				// for canonicalizing account names
	const PlacementMapfileKeys *keys{nullptr};	// last parsed map file, for Adopt warnings
	bool dry_run{false};
};

struct PlacementEditOutcome
{
	std::vector<ClassAd> results;		// one PlacementEditResult per operation
	int error_code{0};					// first failure; 0 if the batch committed (or validated)
	std::string error_string;
	bool committed{false};				// false on failure and on a dry run
	long long changeset_id{0};			// 0 if no audit record was written
	std::vector<std::string> audit;		// one line per operation that changed something
};

// Run the batch in one BEGIN IMMEDIATE.  On any failure, or on a dry run,
// everything is rolled back.  Never throws.
void PlacementRunEdit(PlacementDb &db, const std::vector<ClassAd> &ops,
	const PlacementEditContext &ctx, PlacementEditOutcome &outcome);

#endif
