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

#include "condor_common.h"

#include <charconv>

#include "ClassAdLogAccountantDB.h"
#include "condor_debug.h"

static constexpr std::string_view
PrefixForTable(AccountantTable table)
{
	switch (table) {
		case AccountantTable::Acct:     return "Accountant.";
		case AccountantTable::Customer: return "Customer.";
		case AccountantTable::Resource: return "Resource.";
	}
	return "";
}

std::string
ClassAdLogAccountantDB::MakeKey(AccountantTable table, const std::string& key)
{
	auto prefix = PrefixForTable(table);
	return std::string(prefix) + key;
}

bool
ClassAdLogAccountantDB::Initialize(const char* filename)
{
	return log.InitLogFile(filename);
}

void
ClassAdLogAccountantDB::BeginTransaction()
{
	log.BeginTransaction();
}

void
ClassAdLogAccountantDB::CommitTransaction()
{
	log.CommitTransaction();
}

void
ClassAdLogAccountantDB::CommitNondurableTransaction()
{
	log.CommitNondurableTransaction();
}

void
ClassAdLogAccountantDB::SetAttributeInt(AccountantTable table, const std::string& key,
                                        const std::string& attr, int64_t value)
{
	std::string fullKey = MakeKey(table, key);
	if (log.AdExistsInTableOrTransaction(fullKey) == false) {
		LogNewClassAd* l = new LogNewClassAd(fullKey.c_str(), "*");
		log.AppendLog(l);
	}
	char buf[24] = { 0 };
	std::to_chars(buf, buf + sizeof(buf) - 1, value);
	LogSetAttribute* l = new LogSetAttribute(fullKey.c_str(), attr.c_str(), buf);
	log.AppendLog(l);
}

void
ClassAdLogAccountantDB::SetAttributeFloat(AccountantTable table, const std::string& key,
                                          const std::string& attr, double value)
{
	std::string fullKey = MakeKey(table, key);
	if (log.AdExistsInTableOrTransaction(fullKey) == false) {
		LogNewClassAd* l = new LogNewClassAd(fullKey.c_str(), "*");
		log.AppendLog(l);
	}
	char buf[255];
	snprintf(buf, sizeof(buf), "%f", value);
	LogSetAttribute* l = new LogSetAttribute(fullKey.c_str(), attr.c_str(), buf);
	log.AppendLog(l);
}

void
ClassAdLogAccountantDB::SetAttributeString(AccountantTable table, const std::string& key,
                                           const std::string& attr, const std::string& value)
{
	std::string fullKey = MakeKey(table, key);
	if (log.AdExistsInTableOrTransaction(fullKey) == false) {
		LogNewClassAd* l = new LogNewClassAd(fullKey.c_str(), "*");
		log.AppendLog(l);
	}
	std::string quoted;
	formatstr(quoted, "\"%s\"", value.c_str());
	LogSetAttribute* l = new LogSetAttribute(fullKey.c_str(), attr.c_str(), quoted.c_str());
	log.AppendLog(l);
}

bool
ClassAdLogAccountantDB::GetAttributeInt(AccountantTable table, const std::string& key,
                                        const std::string& attr, int& value)
{
	ClassAd* ad;
	std::string fullKey = MakeKey(table, key);
	if (log.table.lookup(fullKey, ad) == -1) return false;
	if (ad->LookupInteger(attr, value) == 0) return false;
	return true;
}

bool
ClassAdLogAccountantDB::GetAttributeInt(AccountantTable table, const std::string& key,
                                        const std::string& attr, long& value)
{
	ClassAd* ad;
	std::string fullKey = MakeKey(table, key);
	if (log.table.lookup(fullKey, ad) == -1) return false;
	if (ad->LookupInteger(attr, value) == 0) return false;
	return true;
}

bool
ClassAdLogAccountantDB::GetAttributeInt(AccountantTable table, const std::string& key,
                                        const std::string& attr, long long& value)
{
	ClassAd* ad;
	std::string fullKey = MakeKey(table, key);
	if (log.table.lookup(fullKey, ad) == -1) return false;
	if (ad->LookupInteger(attr, value) == 0) return false;
	return true;
}

bool
ClassAdLogAccountantDB::GetAttributeFloat(AccountantTable table, const std::string& key,
                                          const std::string& attr, double& value)
{
	ClassAd* ad;
	std::string fullKey = MakeKey(table, key);
	if (log.table.lookup(fullKey, ad) == -1) return false;
	if (ad->LookupFloat(attr, value) == 0) return false;
	return true;
}

bool
ClassAdLogAccountantDB::GetAttributeString(AccountantTable table, const std::string& key,
                                           const std::string& attr, std::string& value)
{
	ClassAd* ad;
	std::string fullKey = MakeKey(table, key);
	if (log.table.lookup(fullKey, ad) == -1) return false;
	if (ad->LookupString(attr, value) == 0) return false;
	return true;
}

bool
ClassAdLogAccountantDB::DeleteClassAd(AccountantTable table, const std::string& key)
{
	std::string fullKey = MakeKey(table, key);
	ClassAd* ad = NULL;
	if (log.table.lookup(fullKey, ad) == -1)
		return false;

	LogDestroyClassAd* l = new LogDestroyClassAd(fullKey.c_str());
	log.AppendLog(l);
	return true;
}

ClassAd*
ClassAdLogAccountantDB::GetClassAd(AccountantTable table, const std::string& key)
{
	ClassAd* ad = NULL;
	std::string fullKey = MakeKey(table, key);
	std::ignore = log.table.lookup(fullKey, ad);
	return ad;
}

void
ClassAdLogAccountantDB::ForEachInTable(AccountantTable table, ForEachCallback callback)
{
	auto prefix = PrefixForTable(table);
	std::string HK;
	ClassAd* ad;
	log.table.startIterations();
	while (log.table.iterate(HK, ad)) {
		if (HK.compare(0, prefix.size(), prefix) != 0) continue;
		std::string entityKey = HK.substr(prefix.size());
		if (!callback(entityKey, ad)) break;
	}
}

void
ClassAdLogAccountantDB::Compact()
{
	log.TruncLog();
}
