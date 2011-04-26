/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _HISTORYFILE_H
#define _HISTORYFILE_H

#include <string>
#include <deque>

using std::string;
using std::deque;
using std::pair;

#include "stat_wrapper.h"
#include "CondorError.h"

namespace aviary {
namespace history {
		
struct HistoryEntry
{
	string file;
	long int start; // ftell stream index for record start
	long int stop;  // ftell stream index for record end
	string id;
	int q_date;
	int cluster;
	int proc;
	int status;
	int entered_status;
	string submission;
	string owner;
	string cmd;
	string args1;
	string args2;
	string release_reason;
	string hold_reason;
};

class HistoryFile
{
public:
	typedef deque<HistoryEntry> HistoryEntriesType;
	typedef HistoryEntriesType::const_iterator HistoryEntriesTypeIterator;
	typedef pair<HistoryEntriesTypeIterator,
				 HistoryEntriesTypeIterator> HistoryEntriesTypeIterators;

	HistoryFile(const string name);

	~HistoryFile();

	HistoryFile(const HistoryFile &base);
	HistoryFile & operator=(const HistoryFile &base);

	bool init(CondorError &errstack);

	bool getId(long unsigned int &id);

	HistoryEntriesTypeIterators poll(CondorError &errstack);

	void cleanup();

private:
	string m_name;
	string m_index_name;
	HistoryEntriesType m_entries;
	StatStructType *m_stat;
	FILE *m_file;
	FILE *m_index;

	bool pollIndex(CondorError &errstack);

};

}}

#endif /* _HISTORYFILE_H */
