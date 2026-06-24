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

#ifndef _ACCOUNTANT_DB_H_
#define _ACCOUNTANT_DB_H_

#include "condor_classad.h"

#include <string>
#include <functional>

// The three logical tables stored in the Accountant database.
enum class AccountantTable {
	Acct,       // Singleton global state (e.g. LastUpdateTime)
	Customer,   // Per-customer/group accounting records
	Resource    // Per-resource match records
};

class AccountantDB {
public:
	virtual ~AccountantDB() = default;

	// Lifecycle
	virtual bool Initialize(const char* filename) = 0;

	// Transaction management
	virtual void BeginTransaction() = 0;
	virtual void CommitTransaction() = 0;
	virtual void CommitNondurableTransaction() = 0;

	// Record operations
	virtual void SetAttributeInt(AccountantTable table, const std::string& key,
	                             const std::string& attr, int64_t value) = 0;
	virtual void SetAttributeFloat(AccountantTable table, const std::string& key,
	                               const std::string& attr, double value) = 0;
	virtual void SetAttributeString(AccountantTable table, const std::string& key,
	                                const std::string& attr, const std::string& value) = 0;

	virtual bool GetAttributeInt(AccountantTable table, const std::string& key,
	                             const std::string& attr, int& value) = 0;
	virtual bool GetAttributeInt(AccountantTable table, const std::string& key,
	                             const std::string& attr, long& value) = 0;
	virtual bool GetAttributeInt(AccountantTable table, const std::string& key,
	                             const std::string& attr, long long& value) = 0;
	virtual bool GetAttributeFloat(AccountantTable table, const std::string& key,
	                               const std::string& attr, double& value) = 0;
	virtual bool GetAttributeString(AccountantTable table, const std::string& key,
	                                const std::string& attr, std::string& value) = 0;

	virtual bool DeleteClassAd(AccountantTable table, const std::string& key) = 0;
	virtual ClassAd* GetClassAd(AccountantTable table, const std::string& key) = 0;

	// Iteration - calls callback(entityKey, ad) for each record in table.
	// Callback returns true to continue, false to stop.
	using ForEachCallback = std::function<bool(const std::string&, ClassAd*)>;
	virtual void ForEachInTable(AccountantTable table, ForEachCallback callback) = 0;

	// Maintenance
	virtual void Compact() = 0;
};

#endif
