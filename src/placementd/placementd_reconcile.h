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

#ifndef PLACEMENTD_RECONCILE_H
#define PLACEMENTD_RECONCILE_H

// Mapfile reconcile: make the rows owned by the map files
// (source = 'mapfile') equal the files' contents, without touching rows
// the database owns.  Bad or conflicting input is resolved toward less
// access: it never keeps access the file revokes.

#include "placementd_db.h"

#include <set>

// Cap on the Problems list.
constexpr size_t PLACEMENTD_MAX_PROBLEMS = 200;

struct PlacementProblem
{
	std::string kind;
	std::string file;
	int line{0};
	std::string key;
	std::string message;
};

struct PlacementMapfileStatus
{
	time_t load_time{0};		// last successful apply
	time_t attempt_time{0};		// last attempt (including skipped ones)
	bool ok{false};				// last attempt applied with no problems
	int quarantined{0};			// keys whose bad input was not applied
	int conflicts{0};			// internal rows the file reclaimed
	std::vector<PlacementProblem> problems;

	// The status attributes published in the daemon ad, plus
	// PlacementdApiVersion.
	void Publish(ClassAd &ad) const;
	// Publish() plus the Problems list.
	void PublishProblems(ClassAd &ad) const;
	void Save(PlacementDb &db) const;
	void Load(PlacementDb &db);
};

// Keys named by the last parsed files; used to warn that an Adopt will be
// reclaimed by the next reconcile.
struct PlacementMapfileKeys
{
	bool valid{false};
	std::set<std::string> identities;
	std::set<std::string> accounts;
	std::set<std::string, classad::CaseIgnLTStr> projects;
	std::set<std::string, classad::CaseIgnLTStr> authorizations;
	// Names that user lines refer to (without defining them), and the
	// accounts whose lines do.  A reconcile gives each such account a grant
	// of the name, creating a placeholder if no row has it.
	std::map<std::string, std::set<std::string>, classad::CaseIgnLTStr> project_refs;
	std::map<std::string, std::set<std::string>, classad::CaseIgnLTStr> authorization_refs;
};

class PlacementReconciler
{
public:
	explicit PlacementReconciler(PlacementDb &db) : m_db(db) {}

	// Reconcile the map files into the database.  user_mapfile empty means
	// reconciliation is disabled; authz_mapfile empty means there is no
	// authorizations file.  Skips the run when neither file changed since
	// the last successful one.  Returns true if the database was examined
	// (i.e. the run was not skipped).
	bool Run(const std::string &user_mapfile, const std::string &authz_mapfile,
		const std::string &uid_domain);

	// Forget the file signatures so the next Run() does a full pass.  Called
	// after an edit, so that an Adopt of a key still in the file is
	// reclaimed without waiting for the file to change.
	void Invalidate() { m_last_signature.clear(); }

	const PlacementMapfileStatus &Status() const { return m_status; }
	PlacementMapfileStatus &Status() { return m_status; }
	const PlacementMapfileKeys &Keys() const { return m_keys; }

private:
	PlacementDb &m_db;
	std::string m_last_signature;
	PlacementMapfileStatus m_status;
	PlacementMapfileKeys m_keys;
};

#endif
