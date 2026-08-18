/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef _CLASSAD_LOG_ACCOUNTANT_DB_H_
#define _CLASSAD_LOG_ACCOUNTANT_DB_H_

#include "AccountantDB.h"
#include "classad_log.h"

class ClassAdLogAccountantDB : public AccountantDB {
public:
	bool Initialize(const char* filename) override;

	void BeginTransaction() override;
	void CommitTransaction() override;
	void CommitNondurableTransaction() override;

	void SetAttributeInt(AccountantTable table, const std::string& key,
	                     const std::string& attr, int64_t value) override;
	void SetAttributeFloat(AccountantTable table, const std::string& key,
	                       const std::string& attr, double value) override;
	void SetAttributeString(AccountantTable table, const std::string& key,
	                        const std::string& attr, const std::string& value) override;

	bool GetAttributeInt(AccountantTable table, const std::string& key,
	                     const std::string& attr, int& value) override;
	bool GetAttributeInt(AccountantTable table, const std::string& key,
	                     const std::string& attr, long& value) override;
	bool GetAttributeInt(AccountantTable table, const std::string& key,
	                     const std::string& attr, long long& value) override;
	bool GetAttributeFloat(AccountantTable table, const std::string& key,
	                       const std::string& attr, double& value) override;
	bool GetAttributeString(AccountantTable table, const std::string& key,
	                        const std::string& attr, std::string& value) override;

	bool DeleteClassAd(AccountantTable table, const std::string& key) override;
	ClassAd* GetClassAd(AccountantTable table, const std::string& key) override;

	void ForEachInTable(AccountantTable table, ForEachCallback callback) override;

	void Compact() override;

private:
	static std::string MakeKey(AccountantTable table, const std::string& key);

	ClassAdLog<std::string, ClassAd*> log;
};

#endif
