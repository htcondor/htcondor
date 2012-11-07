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

#ifndef _ODS_HISTORY_FILE_H
#define _ODS_HISTORY_FILE_H

#include "ODSMongodbOps.h"

// c++ includes
#include <string>
#include <deque>

// condor include
#include "stat_wrapper.h"
#include "CondorError.h"

namespace plumage {
namespace history {
    
struct HistoryEntry {
    std::string file;
    long int start; // ftell stream index for record start
    long int stop;  // ftell stream index for record end
};

class ODSHistoryFile
{
public:
    typedef std::deque<HistoryEntry> ODSHistoryEntryType;
    typedef ODSHistoryEntryType::const_iterator ODSHistoryEntryTypeIterator;
    typedef std::pair<ODSHistoryEntryTypeIterator,
                 ODSHistoryEntryTypeIterator> ODSHistoryEntryTypeIterators;
    
	ODSHistoryFile(const std::string name);
	~ODSHistoryFile();
	ODSHistoryFile(const ODSHistoryFile &base);
	ODSHistoryFile & operator=(const ODSHistoryFile &base);

	bool init(CondorError &errstack);
	bool getId(long unsigned int &id);
	void cleanup();
    
    ODSHistoryEntryTypeIterators poll(CondorError &errstack);

private:
	std::string m_name;
    ODSHistoryEntryType m_entries;
	StatStructType *m_stat;
	FILE *m_file;

};

}}

#endif /* _ODS_HISTORY_FILE_H */
