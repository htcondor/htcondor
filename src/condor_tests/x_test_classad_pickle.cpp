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
#include "condor_config.h"
#include "match_prefix.h"
#include "dprintf_internal.h"
#include <stdlib.h>
#include <vector>
#include <string>
#include <forward_list>
#include <time.h>
#include "my_async_fread.h"
#include "wire_ad_buffer.h"
#include "../condor_procapi/procapi.h" // for getting cpu time & process memory
#include "../classad/classad/classadCache.h" // for CachedExprEnvelope stats
// so we can query collectors for ads
#include "dc_collector.h"
#include "dc_schedd.h"
#include "condor_io.h"
#include "condor_adtypes.h"

int do_unit_tests(FILE* out, std::vector<const char *> & tests, int options);

#define NUMELMS(aa) (int)(sizeof(aa)/sizeof((aa)[0]))

// turn off some GCC warnings that are unhelpful
GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(unused-but-set-variable)
GCC_DIAG_OFF(unused-parameter)
GCC_DIAG_OFF(unused-variable)
GCC_DIAG_OFF(unused-result)
GCC_DIAG_OFF(unused-function)

#include "../classad/parse_classad_testdata.hpp"
class adsource {
public:
	adsource() : ix(0), ad_index(0), cluster(0), proc(0), addr(0), sock1(0), sock2(0), machine(0), user(0), slots(4) { now = time(NULL); }
	bool fail() { return false; }
	bool eof() const { return ix >= NUMELMS(ad_data); }
	//bool get_line(std::string & buffer);
	bool get_pair(std::string_view & attr, std::string_view & rhs, classad::Value & val);
	int urand() { int r = rand(); return r < 0 ? -r : r; }
protected:
	time_t now;
	size_t ix;
	int    ad_index;
	int    cluster;
	int    proc;
	int    addr;
	int    sock1;
	int    sock2;
	int    machine;
	int    user;
	int    slots;
	char kvpbuf[longest_kvp];
};


//bool getline(adsource & src, std::string & buffer) { return src.get_line(buffer); }

bool adsource::get_pair(std::string_view & attr, std::string_view & rhs, classad::Value & value)
{
	attr = std::string_view();
	if (eof()) return false;
	int attr_pos = ad_data[ix++];
	int val_pos = ad_data[ix++];
	if (attr_pos < 0 || attr_pos > (int)sizeof(attrs)) {
		++ad_index;
		user = cluster = proc = 0;
		if (--slots <= 0) {
			addr = sock1 = sock2 = machine = 0;
			slots = 1+ urand()%9;
		}
		return true;
	}

	static const char * users[] = { "alice", "bob", "james", "sally", "chen", "smyth", "avacado", "dezi", "smoller", "ladyluck", "porter" };

	attr = &attrs[attr_pos];
	const char * p = "UNDEFINED";
	value.SetErrorValue();
	if (val_pos <= (int)sizeof(rhpool)) {
		p = &rhpool[val_pos];
		if (*p == '?') { // special cases
			switch (p[1]) {
			case 'c': //?cid\0" //247,12476
				snprintf(kvpbuf, sizeof(kvpbuf), "\"<128.104.101.22:9618>#%lld#%4d#...\"", (long long)now-(60*60*48), urand()%10000);
				break;
			case 'C': //?Cvmsexist\0" //248,12481
				snprintf(kvpbuf, sizeof(kvpbuf), "ifThenElse(isUndefined(LastHeardFrom),CurrentTime,LastHeardFrom) - %lld < 3600", (long long)now-(60*60*48));
				break;
			case 'g': //?gjid\0" //249,12492
				if ( ! cluster) { cluster=urand(); proc=urand()%100; }
				snprintf(kvpbuf, sizeof(kvpbuf), "\"submit-1.chtc.wisc.edu#%u.%u#%lld\"", cluster, proc, (long long)now-(60*60*48));
				break;
			case 'j': //?jid\0" //250,12498
				if ( ! cluster) { cluster=urand(); proc=urand()%100; }
				snprintf(kvpbuf, sizeof(kvpbuf), "\"%u.%u\"", cluster, proc);
				break;
			case 'M': //?MAC\0" //251,12503
			{ int a = rand(), b = rand(); snprintf(kvpbuf, sizeof(kvpbuf), "\"%02X:%02X:%02X:%02X:%02X:%02X\"", a&0xFF, (a>>8)&0xFF, (a>>16)&0xFF, b&0xFF, (b>>8)&0xFF, (b>>16)&0xFF); }
			break;
			case 'm': //?machine\0" //252,12508
				if ( ! machine) { machine = urand()%1000; }
				snprintf(kvpbuf, sizeof(kvpbuf), "\"INFO-MEM-B%03d-W.ad.wisc.edu\"", machine);
				break;
			case 'n': //?name\0" //252,12508
				if ( ! machine) { machine = urand()%1000; }
				snprintf(kvpbuf, sizeof(kvpbuf), "\"slot%d@INFO-MEM-B%03d-W.ad.wisc.edu\"", slots, machine);
				break;
			case 's': {//?sin\0" //253,12517{
				if ( ! addr) { addr = urand(); sock1 = rand(); sock2 = rand(); }
				if ((addr&0x300)==0x300) {
					snprintf(kvpbuf, sizeof(kvpbuf), "\"<144.92.184.%03d:%d?CCBID=128.105.244.14:9620%%3fsock%%%x_%x#5702&PrivAddr=%%3c127.0.0.1:49226%%3e&PrivNet=INFO-MEM-B%03d-W.ad.wisc.edu>\"",
						addr&0xFF, 1000+(addr>>8)%50000, sock1, sock2&0xFFFF, addr%1000);
				} else {
					snprintf(kvpbuf, sizeof(kvpbuf), "\"<144.92.%d.%d:9618?sock=%x_%x\"",
						addr&0xFF, (addr>>8)&0xFF, sock1, sock2&0xFFFF);
				}
			}
					break;
			case 'h': //?hist\0" //254,12522
				if ( ! machine) { machine = urand()%1000; }
				strcpy(kvpbuf, "\"0x00000000000000000000000000000000\"");
				if ((machine%100 < 26)) { kvpbuf[4+(machine%100)] = '1'; }
				break;
			case 'u': //?user\0" //255,12530
				if ( ! user) { user = 1 + urand() % NUMELMS(users); }
				snprintf(kvpbuf, sizeof(kvpbuf), "\"%s@submit.chtc.wisc.edu\"", users[user-1]);
				break;
			default: // ?min,max
				break;
			}
			p = kvpbuf;
		} else {
			switch (val_pos) {
			case 12454: value.SetBooleanValue(false); break;
			case 12467: value.SetBooleanValue(true); break;
			case 12472: value.SetUndefinedValue(); break;
			}
		}
	} else if ((val_pos >= rhint_index_base) && (val_pos - rhint_index_base) < NUMELMS(rhint)) {
		long long lo = rhint[val_pos - rhint_index_base].lo;
		long long hi = rhint[val_pos - rhint_index_base].hi;
		long long val = lo;
		if (hi > lo) { val += urand() % (hi-lo+1); }
		snprintf(kvpbuf, sizeof(kvpbuf), "%lld", val);
		value.SetIntegerValue(val);
		p = kvpbuf;
	} else if ((val_pos >= rhdouble_index_base) && (val_pos - rhdouble_index_base) < NUMELMS(rhdouble)) {
		double lo = rhdouble[val_pos - rhdouble_index_base].lo;
		double hi = rhdouble[val_pos - rhdouble_index_base].hi;
		double val = lo;
		if (hi > lo) { val += (1.0/RAND_MAX) * urand() * (hi-lo); }
		snprintf(kvpbuf, sizeof(kvpbuf), "%f", val);
		value.SetRealValue(val);
		p = kvpbuf;
	}

	rhs = p;
	return true;
}

int  dash_verbose = 0;
int  dash_limit = 0; // used by unit tests
bool dash_pickle = false; // used by unit tests
bool dash_dump = false;
bool generate_some = false;
bool generate_binary = false;
bool generate_wireline = false;
bool skip_list_exprs = false;
FILE* generate_fp = NULL;
int   updates_sent = 0;

// returns the length of the string including quotes
// if str starts and ends with doublequotes
// and contains no internal \ or doublequotes.
static size_t IsSimpleString( const char *str )
{
	if ( *str != '"') return false;

	++str;
	size_t n = strcspn(str,"\\\"");
	if  (str[n] == '\\') {
		return 0;
	}
	if (str[n] == '"') { // found a close quote - good so far.
						 // trailing whitespace is permitted (but leading whitespace is not)
						 // return 0 if anything but whitespace follows
						 // return length of quoted string (including quotes) if just whitespace.
		str += n+1;
		for (;;) {
			char ch = *str++;
			if ( ! ch) return n+2;
			if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') return 0;
		}
	}

	return 0;
}

static long long myatoll(const char *s, const char* &end) {
	long long result = 0;
	int negative = 0;
	if (*s == '-') {
		negative = 1;
		s++;
	}

	while ((*s >= '0') && (*s <= '9')) {
		result = (result * 10) - (*s - '0');
		s++;
	}
	end = s;
	if (!negative) {
		result = -result;
	}
	return result;
}

class MyAioDataStream : public AioDataStream
{
public:
	MyAioDataStream(MyAsyncFileReader & _aio) : AioDataStream(_aio) {};
	~MyAioDataStream() {};

	bool peekByte(unsigned char &by) {
		const char * p1 = nullptr;
		const char * p2 = nullptr;
		int c1=0, c2 = 0;
		
		if ( ! waitForData(p1, c1, p2, c2)) return false;
		if (p1 && c1) { by=p1[0]; } else if (p2 && c2) { by = p2[0]; }
		return true;
	}

	bool readByte(unsigned char &by) {
		if ( ! peekByte(by)) return false;
		consume_data(1);
		return true;
	}

	unsigned int readBytes(unsigned char *buf, unsigned int len) {
		const char * p1 = nullptr;
		const char * p2 = nullptr;
		int c1=0, c2 = 0;
		unsigned int ret = 0;
		while (waitForData(p1, c1, p2, c2)) {
			unsigned int last_ret = ret;
			for (int ix = 0; (ret < len) && (ix < c1); ++ix) { buf[ret++] = p1[ix]; }
			if (ret < len && p2) {
				for (int ix = 0; (ret < len) && (ix < c2); ++ix) { buf[ret++] = p2[ix]; }
			}
			consume_data(ret - last_ret);
			if (ret == len) {
				break;
			}
		}
		return ret;
	}

	int peekData(std::string_view & str) {
		const char * p1 = nullptr;
		const char * p2 = nullptr;
		int c1=0, c2 = 0;
		if ( ! waitForData(p1, c1, p2, c2)) return 0;
		str = std::string_view(p1, c1);
		return c1 + c2;
	}

	int peekData(std::string_view & str1, std::string_view & str2) {
		const char * p1 = nullptr;
		const char * p2 = nullptr;
		int c1=0, c2 = 0;
		if ( ! waitForData(p1, c1, p2, c2)) return 0;
		str1 = std::string_view(p1, c1);
		str2 = std::string_view(p2, c2);
		return c1 + c2;
	}

private:
	bool waitForData(const char * & p1, int & cb1, const char * & p2, int & cb2) {
		int sleep_time = 0;
		for (;;) {
			if (get_data(p1, cb1, p2, cb2)) {
				return true;
			} else if (done_reading()) {
				return false;
			} else {
				sleep(sleep_time);
				sleep_time += 1;
			}
		}
		return false;
	}
};

static classad::ExprStreamMaker ad_maker;
static std::string ad_label;

static bool save_ad(ClassAdList & ads, ClassAd * ad, int /*id*/)
{
	ads.Insert(ad);
	return true;
}

static bool save_wire_ad(std::list<std::u8string> & ads, WireClassadBuffer &wab, int /*id*/)
{
	ads.emplace_back((const char8_t *)wab.data(), wab.size());
	return true;
}

static bool save_and_check_wire_ad(std::list<std::u8string> & ads, WireClassadBuffer &wab, int id)
{
	ClassAd ad;
	auto stmAd = wab.stream();
	if ( ! ad.UpdateViaCache(stmAd)) {
		fprintf(stderr, "ClassAd::UpdateViaCache [%d] failed\n", id);
	}
	auto keys = wab.keys();
	if ((int)keys.size() != ad.size()) {
		fprintf(stderr, "ClassAd::UpdateViaCache [%d] size %d does not match index size %d\n",
			id, (int)ad.size(), (int)keys.size());
	}
	if (dash_verbose) fprintf(stderr, "save_and_check_wire_ad [%d] attrs:%d keys:%d\n", id, (int)ad.size(), (int)keys.size());
	for (auto & sv : keys) {
		// TODO: fix to use the string_view for lookup once classads supports that.
		std::string key(sv);
		if ( ! ad.Lookup(key)) {
			fprintf(stderr, "ad is missing key %s\n", key.c_str());
		}
	}

	ads.emplace_back((const char8_t *)wab.data(), wab.size());
	return true;
}

static int cTotalAds = 0;
static int cTotalAttributes = 0;
static int cSkippedAttributes = 0;

int parse_ads(
	ClassAdList &ads,
	bool (*fn)(ClassAdList &ads, ClassAd* ad, int id),
	unsigned int seed, bool lazy = false, bool smart = false, int ad_limit = 1000000)
{
	bool with_cache = classad::ClassAdGetExpressionCaching();
	int barf_counter = 0;
	int rval = 0;

	srand(seed);
	adsource infile;

	std::string name, szValue, linebuf, attr;
	std::string_view lhs, rhs;
	linebuf.reserve(longest_kvp+100);

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);
	classad::Value value;

	ClassAd * cad = NULL;
	while (!infile.fail() && !infile.eof())
	{
		infile.get_pair(lhs, rhs, value);

		// This is the end of an ad
		if (lhs.empty()) {
			++cTotalAds;
			fn(ads, cad, cTotalAds);
			cad = NULL;
			if (dash_dump) {
				fputs("\n", stdout);
			}
			if (cTotalAds >= ad_limit) {
				break;
			}
			continue;
		}

		++cTotalAttributes;
		if (skip_list_exprs && rhs.front() == '{') {
			++cSkippedAttributes;
			continue;
		}

		if (dash_dump) {
			linebuf = lhs;
			linebuf += " = ";
			linebuf += rhs;
			fputs(linebuf.c_str(), stdout);
			continue;
		}

		if ( ! cad) {
			cad = new ClassAd();
		}

		attr = lhs;

		bool success = false;
		bool inserted = false;
		bool cache_this_one = with_cache;
		if (smart) {
			size_t cbrhs = rhs.size();
			classad::Literal* lit = fastParseSomeClassadLiterals(rhs.data(), cbrhs, 128);
			if (lit) {
				inserted = cad->InsertLiteral(attr, lit);
			} else if (rhs[0] == '[' || rhs[0] == '{') {
				cache_this_one = false;
			}
		}
		// if we did a smart insert, we are done
		if (inserted) {
			success = true;
		} else {
			szValue = rhs;
			if (cache_this_one) {
				success = cad->InsertViaCache(attr, szValue, lazy);
			} else {
				ExprTree *tree = parser.ParseExpression(szValue);
				if ( ! tree) {
					return false;
				} else {
					success = cad->Insert(attr, tree);
				}
			}
		}
		if ( ! success) {
			++barf_counter;
			fprintf(stderr, "BARFED ON: %s = %s\n", attr.c_str(), szValue.c_str());
			if (barf_counter > 1000) {
				fprintf(stderr, "error count exceeds 1000, aborting test\n");
				rval = 1;
				break;
			}
		}
	}

	if (cad && cad->size() > 0) {
		++cTotalAds;
		fn(ads, cad, cTotalAds);
		cad = NULL;
	}

	return rval;
}

int generate_ads(
	std::list<std::u8string> &ads,
	WireClassadBuffer & wab,
	bool (*fn)(std::list<std::u8string> &ads, WireClassadBuffer &wab, int id),
	bool pickled, bool raw_wire = false, unsigned int seed=42, int ad_limit = 1000000)
{
	srand(seed);
	adsource infile;

	std::string linebuf;
	linebuf.reserve(longest_kvp+100);

	std::string_view lhs, rhs;

	int adId = 0;

	int64_t time_and_flags = 0;
	if (pickled) time_and_flags = BINARY_CLASSAD_FLAG + time(NULL);

	wab.clear(time_and_flags);
	auto mk = wab.mark();

	classad::ClassAdParser parser;
	parser.SetOldClassAd( true );
	classad::Value value;

	//std::map<std::string_view, int> attr_counts;

	while (!infile.fail() && !infile.eof())
	{
		infile.get_pair(lhs, rhs, value);

		//attr_counts[lhs] += 1;

		// This is the end of an ad
		if (lhs.empty() && mk.at()) {
			++cTotalAds;
			wab.closeAd(mk);
			fn(ads, wab, adId);
			wab.clear(time_and_flags);
			mk = wab.mark();
			if (ad_limit && (adId >= ad_limit)) break;
			continue;
		}

		//if (skip_list_exprs && strstr(szInput.c_str(), " = {")) {
		//	++cSkippedAttributes;
		//	continue;
		//}
		++cTotalAttributes;
		if (wab.empty() && ! mk.at()) {
			++adId;
			mk = wab.openAd("ad" + std::to_string(adId));
		}

		if (pickled) {
			unsigned int cb = lhs.size();
			if (cb < 256) {
				wab.putByte(classad::ExprStream::EsSmallPair);
				wab.putSmallString(lhs);
			} else {
				wab.putByte(classad::ExprStream::EsBigPair);
				wab.putString(lhs);
			}

			if (value.IsBooleanValue() || value.IsIntegerValue() || value.IsRealValue() || value.IsUndefinedValue()) {
				classad::Literal * lit = classad::Literal::MakeLiteral(value);
				wab.putNullableExpr(lit);
				delete lit;
			} else {
				unsigned int len = IsSimpleString(rhs.data()); // len includes quotes
				if (len >= 2) {
					rhs = rhs.substr(1, len-2);
					len = rhs.size();
					if (len < 256) {
						unsigned char cch = (unsigned char)len;
						wab.putByte(classad::ExprStream::EsString);
						wab.putByte(cch);
						if (len) wab.putBytes(rhs.data(), len);
					} else {
						wab.putByte(classad::ExprStream::EsBigString);
						wab.putInteger(len);
						wab.putBytes(rhs.data(), len);
					}
				} else if (rhs.front() == '"') {
					fprintf(stderr, "unsimple string: %s\n", rhs.data());
				} else {
					classad::ExprTree * tree = nullptr;
					if ( ! parser.ParseExpression(rhs.data(), tree, true)) {
						fprintf(stderr, "bad expr: %s\n", rhs.data());
					} else {
						// fprintf(stderr, "hard expr: %s\n", rhs.data());
						wab.putNullableExpr(tree);
						delete tree;
					}
				}
			}
		} else if (raw_wire) {
			// write the wire line 'pair' tag and the attribute name, in compact or regular form
			linebuf = lhs; linebuf += " = "; linebuf += rhs;
			unsigned int cb = linebuf.size()+1;	// include the null term
			if (cb < 256) {
				wab.putByte(classad::ExprStream::EsSmallEqZPair);
				wab.putByte(cb);
			} else {
				wab.putByte(classad::ExprStream::EsBigEqZPair);
				wab.putInteger(cb);
			}
			wab.putBytes(linebuf.c_str(), cb);
		} else {
			std::string_view rhsz(rhs.data(), rhs.size()+1); // include the null terminator in the rhs 
			wab.putPairUnparsed(lhs, rhsz);
		}
	}

	if ( ! wab.empty()) {
		wab.closeAd(mk);
		fn(ads, wab, adId);
	}

	//for (auto &[key,count] : attr_counts) {
	//	fprintf(stdout, "\t%4d : %s\n", count, key.data());
	//}

	return ads.size();
}

//chtc
// cm.chtc.wisc.edu
// ap2001.chtc.wisc.edu
//ospool
// cm-1.ospool.osg-htc.org,cm-2.ospool.osg-htc.org
// ap40.uw.osg-htc.org
int fetch_ads(std::list<std::u8string> &ads,
	bool (*fn)(std::list<std::u8string> &ads, WireClassadBuffer &wab, int id),
	DCCollector & pool,
	bool pickled) // request pickled ads (future)
{
	ClassAd queryAd;
	SetMyTypeName (queryAd, QUERY_ADTYPE);

	// if (resultLimit > 0) { queryAd.Assign(ATTR_LIMIT_RESULTS, resultLimit); }
	// if ( ! constr.empty()) { queryAd.Insert(ATTR_REQUIREMENTS, constr.Expr()->Copy()); }
	// if (target) { queryAd.Assign(ATTR_TARGET_TYPE, AdTypeToString(queryType));
	if (pickled) { queryAd.Assign("Pickled", true); }

	// 
	// older collectors require a trival Requirements expression, if you don't pass one
	// you get no results
	// TODO: move this to after we know the collector version?
	if ( ! queryAd.Lookup(ATTR_REQUIREMENTS)) {
		queryAd.AssignExpr(ATTR_REQUIREMENTS, "true");
	}

	int mytimeout = 300;
	int cmd = QUERY_STARTD_ADS;
	CondorError errstack;
	WireClassadBuffer wab;
	int adId = 0;

	Sock*    sock; 
	if (!(sock = pool.startCommand(cmd, Stream::reli_sock, mytimeout, &errstack)) ||
		!putClassAd (sock, queryAd) || !sock->end_of_message()) {
		fprintf(stderr, "Failed to query collector %s : %s\n", pool.name(), errstack.getFullText().c_str());
		delete sock;
		return 0;
	}

	// get result
	sock->decode ();
	int more = 1;
	while (more)
	{
		if (!sock->code (more)) {
			sock->end_of_message();
			delete sock;
			break;
		}
		if (more) {
			++adId;
			if( !getClassAdRaw(sock, wab) ) {
				sock->end_of_message();
				fprintf(stderr, "ad stream interrupted at %d\n", adId);
				delete sock;
				break;
			}
			if ( ! wab.empty()) {
				++cTotalAds;
				classad::ExprStream stm = wab.stream();
				std::string_view label;
				wab.find_hash(stm, label);
				cTotalAttributes += wab.read_hash(stm);
				fn(ads, wab, adId);
			}
		}
	}
	sock->end_of_message();

	// finalize
	sock->close();
	delete sock;

	return ads.size();
}

typedef bool (*ScheddQueryCallback)(void*, class WireClassadBuffer & wab);

int fetch_jobs(std::list<std::u8string> &ads,
	bool (*fn)(std::list<std::u8string> &ads, WireClassadBuffer &wab, int id),
	DCSchedd & schedd,
	bool pickled) // request pickled ads (future)
{
	const char * constraint = nullptr;
	const char * projection = nullptr;
	const bool send_server_time = true;
	int limit = -1;
	int opts = 0;

	if (dash_verbose) { fprintf(stderr, "fetch_jobs from %s\n", schedd.name()); }

	CondorError errStack;
	ClassAd * summaryAd = NULL;
	std::vector<ClassAd *> results;

	int query_timeout = param_integer("Q_QUERY_TIMEOUT", 20);
	auto_free_ptr owner;
	//if (opts & fetch_MyJobs) { owner.set(my_username()); }

	// stuff we need to pass to the query callback
	struct query_aggregates {
		bool (*fn)(std::list<std::u8string> &ads, WireClassadBuffer &wab, int id);
		std::list<std::u8string> &ads;
		int total_ads;
		int total_attrs;
		int total_hash_pairs;
	} queryAgg{fn,ads,0,0,0};

	// use raw ad query
	ClassAd query_ad;
	int rv = DCSchedd::makeJobsQueryAd(query_ad, constraint, projection, opts, limit, owner, send_server_time);
	if (rv == Q_OK) {

		if (dash_verbose) { 
			std::string buffer;
			fprintf(stderr, "  query_ad:\n%s", formatAd(buffer, query_ad, "\t", nullptr, false)); 
		}

		int cmd = QUERY_JOB_ADS;
		if (schedd.canUseQueryWithAuth()) cmd = QUERY_JOB_ADS_WITH_AUTH;

		WireClassadBuffer wab;
		rv = schedd.queryJobs(cmd, query_ad,
		#if 1
			// store raw ads
			[](void * pv, WireClassadBuffer & wab) -> bool {
				struct query_aggregates& agg = *((struct query_aggregates*)pv);
				agg.total_ads += 1;
				agg.total_attrs += wab.keys().size();
				agg.total_hash_pairs += wab.index_size();
				return agg.fn(agg.ads, wab, agg.total_ads);
			}, &queryAgg, 
		#else	
			// fetch raw ads and convert immediately to classads
			[](void * pv, WireClassadBuffer & wab) -> bool {
				fn(ads, wab, 0);
				std::vector<ClassAd *>& ads = *((std::vector<ClassAd *>*)pv);
				ads.push_back(new ClassAd());
				return updateAdFromRaw(*ads.back(), wab, 0, nullptr);
			}, &results, 
		#endif
			query_timeout, &errStack, wab);
		if (rv == Q_OK) {
			// the final state of the wab is the content of the done/summary ad
			if (opts == QueryFetchOpts::fetch_SummaryOnly) {
				summaryAd = new ClassAd();
				if ( ! updateAdFromRaw(*summaryAd, wab, 0, nullptr)) {
					rv = Q_REMOTE_ERROR;
				} else {
					// TODO: MyType of the ad should be "Summary"
					summaryAd->Delete(ATTR_OWNER); // remove the bogus owner attribute
				}
			}
		}
	}

	switch( rv ) {
	case Q_OK:
		cTotalAds = queryAgg.total_ads;
		cTotalAttributes = queryAgg.total_attrs;
		break;

	case Q_PARSE_ERROR:
	case Q_INVALID_CATEGORY:
		// This was ClassAdParseError in version 1.
		fprintf(stderr, "Parse error in constraint, %s\n", constraint?constraint:"<null>");
		return 0;

	case Q_UNSUPPORTED_OPTION_ERROR:
		// This was HTCondorIOError in version 1.
		fprintf(stderr, "Query fetch option unsupported by this schedd. 0x%x", opts);
		return 0;

	default:
		// This was HTCondorIOError in version 1.
		std::string error = errStack.getFullText();
		fprintf(stderr, "Failed to fetch ads from schedd, code=%d, errmsg=%s\n", rv, error.c_str());
		return 0;
	}

	return (int)ads.size();
}

int load_ads(std::list<std::u8string> &ads,
	bool (*fn)(std::list<std::u8string> &ads, WireClassadBuffer &wab, int id),
	const char * filename,
	const char* & mode)
{
	fprintf(stdout, "load from %s\n", filename);

	MyAsyncFileReader reader; reader.set_sync(true);

	int err = reader.open(filename);
	if (err) {
		fprintf(stderr, "could not open %s : %d - %s\n", filename, err, strerror(err));
		return 0;
	} else {
		err = reader.queue_next_read();
		if (err) {
			fprintf(stderr, "could not initiate reading from %s, error = %d - %s", filename, err, strerror(err));
			return 0;
		}
	}

	int adId = 0;
	WireClassadBuffer wab; wab.grow(8192);
	auto mk = wab.mark();
	unsigned char ct = 0;

	MyAioDataStream aiods(reader);
	aiods.peekByte(ct);
	bool binary = (ct == classad::ExprStream::EsClassAd);
	int64_t time_and_flags = binary ? BINARY_CLASSAD_MASK : 0;

	if (binary) {

		mode = "Binary";
		bool check_hash_payload = true;

		while (aiods.readByte(ct) && (ct == classad::ExprStream::EsClassAd)) {

			wab.clear(time_and_flags);

			// read the classad header label
			unsigned int label_len=0;
			if (sizeof(label_len) == aiods.readBytes((unsigned char*)&label_len, sizeof(label_len))) {
				wab.putByte(ct);
				wab.putInteger(label_len);
				unsigned char * label_buf = wab.appendBuffer(label_len);
				unsigned int red = aiods.readBytes(label_buf, label_len);
				if (red < label_len) {
					fprintf(stderr, "short read for ad label\n");
				}
			}
			// read hash payload size
			unsigned int hash_size=0;
			if (sizeof(hash_size) == aiods.readBytes((unsigned char*)&hash_size, sizeof(hash_size))) {
				wab.putInteger(hash_size);
				unsigned char * hash_buf = wab.appendBuffer(hash_size);
				unsigned int red = aiods.readBytes(hash_buf, hash_size);
				if (red < hash_size) {
					fprintf(stderr, "short read for ad hash blob\n");
				}

				if (check_hash_payload) {
					check_hash_payload = false; // do this check only for the first ad
					classad::ExprStream stm(hash_buf, hash_size);
					if (wab.read_hash(stm) > 0) {
						std::string_view attr;
						classad::ExprTree::NodeKind kind;
						binary_span expr;
						if (wab.next_pair(stm, attr, expr, kind)) {
							if (kind == classad::ExprTree::EXPR_ENVELOPE) {
								mode = "WirePair";
							}
						}
					}
				}
			}

			//fprintf(stdout, "binary ad %p size %d (0x%x)\n", wab.data(), (int)wab.size(), (int)wab.size());
			if ( ! wab.empty()) {
				++cTotalAds;
				fn(ads, wab, adId);
			}
		}

	} else if (ct == classad::ExprStream::EsBigHash || ct == classad::ExprStream::EsHash) {

		mode = "WirePair";

		while (aiods.readByte(ct) && (ct == classad::ExprStream::EsBigHash || ct == classad::ExprStream::EsHash)) {

			wab.clear(0);

			unsigned int attr_count = 0;
			std::string attr;
			if (ct == classad::ExprStream::EsHash) {
				unsigned char count = 0;
				aiods.readByte(count);
				attr_count = count;
				wab.putByte(ct);
				wab.putByte(count);
			} else { // classad::ExprStream::BigHash
				aiods.readBytes((unsigned char*)&attr_count, sizeof(attr_count));
				wab.putByte(ct);
				wab.putInteger(attr_count);
			}

			for (unsigned int ix = 0; ix < attr_count; ++ix) {
				if ( ! aiods.readByte(ct)) break;

				wab.putByte(ct);

				unsigned char attr_len = 0;
				if (ct == classad::ExprStream::EsSmallPair) {
					unsigned char len = 0;
					aiods.readByte(len);
					wab.putByte(len);
					attr_len = len;
				} else if (ct == classad::ExprStream::EsBigPair) { // BigPair
					aiods.readBytes((unsigned char*)&attr_len, sizeof(attr_len));
					wab.putInteger(attr_len);
				} else {
					break;
				}
				// copy attribute name
				unsigned char * attr_buf = wab.appendBuffer(attr_len);
				unsigned int red = aiods.readBytes(attr_buf, attr_len);
				if (red != attr_len) {
					break;
				}

				// copy expr
				std::string_view d1;
				if ( ! aiods.peekData(d1)) {
					fprintf(stdout, "no data for expr\n");
					break;
				} else {
					int err=0;
					std::string db;
					classad::ExprStream stm(d1.data(), d1.size());
					classad::ExprTree::NodeKind kind;
					unsigned int expr_len = classad::ExprTree::Scan(stm, kind, true, &err);
					if ( ! expr_len) {
						std::string_view d2;
						if ( ! aiods.peekData(d1, d2)) {
							fprintf(stdout, "no more data for expr\n");
							break;
						}
						//fprintf(stdout, "could not scan expr %d, err=%d, retrying with joined buffer %d+%d\n", kind, err, (int)d1.size(), (int)d2.size());
						db.reserve(d1.size() + d2.size()+1);
						db = d1; db += d2;
						d1 = std::string_view(db.data(), db.size());
						classad::ExprStream stm(d1.data(), d1.size());
						expr_len = classad::ExprTree::Scan(stm, kind, true, &err);
						if ( ! expr_len) {
							fprintf(stdout, "scan retry of expr %d failed, err=%d\n", kind, err);
							break;
						}
						//fprintf(stdout, "expr %d, size %d\n", kind, expr_len);
					}
					unsigned char * expr_buf = wab.appendBuffer(expr_len);
					red = aiods.readBytes(expr_buf, expr_len);
					if (red != expr_len) {
						fprintf(stdout, "could not copy %d expr %d bytes to wab, copied %d\n", expr_len, kind, red);
						break;
					}
				}
			}

			//fprintf(stdout, "hash %p size %d (0x%x)\n", wab.data(), (int)wab.size(), (int)wab.size());
			if ( ! wab.empty()) {
				++cTotalAds;
				fn(ads, wab, ++adId);
			}
		}

	} else {

		ClassAd dummy;
		CondorClassAdFileParseHelper helper("\n");
		std::string line;
		std::string_view attr;
		const char * rhs;

		mode = "Long";

		while(reader.readline(line)) {
			switch (helper.PreParse(line, dummy, nullptr)) {
				case 0:  // skip
					continue;
				case 1:  // parse as attr=value
					++cTotalAttributes;
					if (wab.empty() && ! mk.at()) {
						mk = wab.openAd("");
					}
					SplitLongFormAttrValue(line.c_str(), attr, rhs);
					// save the rhs include the null terminator
					wab.putPairUnparsed(attr, std::string_view(rhs, strlen(rhs)+1));
					break;
				case 2:  // ad separator
					if ( ! wab.empty()) {
						wab.closeAd(mk);
						++cTotalAds;
						fn(ads, wab, ++adId);
						wab.clear(0);
						mk = wab.mark(); // this clears the mark.
					}
					break;
				default: // abort
					reader.clear();
					break;
			}
		}

		if ( ! wab.empty()) {
			wab.closeAd(mk);
			++cTotalAds;
			fn(ads, wab, ++adId);
			wab.clear(0);
			mk = wab.mark(); // this clears the mark.
		}
	}

	reader.close();

	return ads.size();
}

bool smartUpdateViaCache(ClassAd & ad, classad::ExprStream & stm, bool lazy)
{
	unsigned int attr_count = 0, ix = 0;
	std::string attr;
	std::string_view line;
	ExprTree * tree = nullptr;
	const char * rhs = nullptr;

	classad::ExprStream::Mark mk = stm.mark();

	unsigned char ct = 0;
	if (! stm.readByte(ct)) { goto bail; }
	if (ct == classad::ExprStream::EsHash) {
		unsigned char count = 0;
		if (! stm.readByte(count)) { goto bail; }
		attr_count = count;
	} else if (ct == classad::ExprStream::EsBigHash) {
		if (! stm.readInteger(attr_count)) { goto bail; }
	} else {
		goto bail;
	}

	ad.rehash(attr_count + 5);
	for (ix = 0; ix < attr_count; ++ix) {
		if (! stm.readByte(ct)) { goto bail; }

		if (ct == classad::ExprStream::EsSmallPair || ct == classad::ExprStream::EsBigPair) {
			if (! stm.readSizeString(attr, ct == classad::ExprStream::EsSmallPair)) { goto bail; }
			// special case for unparsed expressions, we want to use the fast parsing tricks on them.
			if (stm.peekByte(ct) && (ct == classad::ExprStream::EsEnvelope || ct == classad::ExprStream::EsBigEnvelope)) {
				stm.readByte(ct);
				stm.readSizeString(line, ct == classad::ExprStream::EsEnvelope); // line is an unparsed expression here
				classad::Literal * lit = fastParseSomeClassadLiterals(line.data(), line.size(), 128);
				if (lit) { ad.InsertLiteral(attr, lit); }
				else { ad.InsertViaCache(attr, line.data(), lazy); }
			} else if (stm.readNullableExpr(tree)) {
				ad.InsertViaCache(attr,tree); 
			} else {
				goto bail;
			}
		} else if (ct == classad::ExprStream::EsComment || ct == classad::ExprStream::EsBigComment) {
			if (! stm.skipComments()) { goto bail; }
			--ix; // this doesn't count as an attribute
		} else if (ct == classad::ExprStream::EsSmallEqZPair || ct == classad::ExprStream::EsBigEqZPair) {
			// the string here is actually "attr = rhs", which we load into attr
			if ( ! stm.readSizeString(line, ct == classad::ExprStream::EsSmallEqZPair)) { goto bail; }
			if (SplitLongFormAttrValue(line.data(), attr, rhs)) {
				unsigned int cbrhs = line.size() - (rhs - line.data());
				classad::Literal* lit = fastParseSomeClassadLiterals(rhs, cbrhs, 128);
				if (lit) {
					ad.InsertLiteral(attr, lit);
				} else {
					ad.InsertViaCache(attr, rhs, lazy);
				}
			}
		} else {
			goto bail;
		}
	}

	return true;
bail:
	stm.unwind(mk);
	return false;
}

ClassAd * smartMakeClassAd(classad::ExprStream & stm, bool lazy, std::string * label, std::string * comment)
{
	ClassAd * cad = new ClassAd();
	unsigned char ct = 0;
	if (stm.peekByte(ct) && ct == classad::ExprStream::EsClassAd) {
		stm.readByte(ct);
		if (label) {
			if (! stm.readString(*label)) { goto bail; }
		} else {
			if (! stm.skipStream()) { goto bail; }
		}
		classad::ExprStream stm2;
		if (! stm.readStream(stm2)) { goto bail; }
		if (comment) {
			stm2.readOptionalComment(*comment, false);
		} else {
			stm2.skipComments();
		}
		if (! smartUpdateViaCache(*cad, stm2, lazy)) { goto bail; }
	} else {
		if (comment) {
			stm.readOptionalComment(*comment, false);
		} else {
			stm.skipComments();
		}
		if (! smartUpdateViaCache(*cad, stm, lazy)) { goto bail; }
	}
	return cad;

bail:
	delete cad;
	return NULL;

}

static void captureAndPrintUsage(
	const char * mode,
	const char * label,
	const char * cache,
	double & tmAfter,
	size_t & cbAfter)
{
	double tmStep = tmAfter;

	size_t cbStep = ProcAPI::getBasicUsage(getpid(), &tmStep);
	std::string growmsg;
	if (cbStep > cbAfter) {
		formatstr(growmsg, "grew   %.4f MBytes", (cbStep - cbAfter)/(1024*1024.0));
	} else if (cbStep < cbAfter) {
		formatstr(growmsg, "shrunk %.4f MBytes", (cbAfter - cbStep)/(1024*1024.0));
	} else {
		formatstr(growmsg, "no mem change");
	}
	fprintf(stdout, "%s %6s Time: %8.6f CPU-sec,%s %s (from %" PRId64 " to %" PRId64 ")\n",
		mode, label, (tmStep - tmAfter), cache, growmsg.c_str(), (PRId64_t)cbAfter, (PRId64_t)cbStep);
	tmAfter = tmStep; cbAfter = cbStep;
}

// ClassAd loading, processing and performance testing of the same
// 
//    1) a list of unparsed binary Ad blobs can be generated, queried or loaded from a file (generate, load_from, query_from)
//    2) The unparsed ad blobs can optionally be saved to a file (save_to)
//    3) The list of ads blobs an then be parsed into a ClassAdList with or without the classad cache
//      A) parsed ads can be pickled and saved to a file (pickle_to)
//      B) parsed ads can be printed to a file (print_to)
//    4) parsed ads and unparsed ad blobs are freed
//
// Cpu usage and process memory is measured and reported after each step.
// 
// Note that generating pickled ads is done by parsing and pickling each attr,expr pair because the generation data is all text.
//
int main(int argc, const char ** argv)
{
	const char * pcolon = NULL;
	const char * load_from = nullptr;   // read ads from file
	const char * pickle_to = nullptr;   // write pickeled ads in addition to saving the raw generated/pickled/loaded ads
	const char * print_to = nullptr;    // print the parsed adlist to a file

	bool show_usage = false;

	bool parse_generated_ads = false; // parse generated/loaded/queried ads
	bool with_cache = false;          // enable classad cache, and parse though cache
	int  cache_string_size = -1;
	bool lazy = false;                // cache lazy
	bool smart = false;               // cache smart (i.e. don't let the cache see simple literals)

	const char * advertise_to = NULL;
	bool advertise_pickled = false;
	DCCollector * advertise_pool = NULL;
	bool use_tcp = true;

	const char * query_from = nullptr;
	bool query_pickled = false;
	bool query_jobs = false;
	DCCollector * query_pool = NULL;
	DCSchedd * query_schedd = NULL;

	const char * save_to = nullptr;
	bool save_ads = true;
	bool save_pickled = false;
	bool save_hashes = false;
	bool save_long = false;
	
	int  limit = 1000000;
	int  rval = 0;
	unsigned int seed = 42;

	bool dash_unittest = false;
	std::vector<const char *> testnames;

	int ix = 1;
	while (ix < argc) {
		if (is_dash_arg_prefix(argv[ix], "help", -1)) {
			show_usage = true;
			break;
		} else if (is_dash_arg_colon_prefix(argv[ix], "cache", &pcolon, 1)) {
			with_cache = true;
			if (pcolon && isdigit(pcolon[1])) {
				cache_string_size = atoi(pcolon+1);
			}
		} else if (is_dash_arg_prefix(argv[ix], "nocache", 3)) {
			with_cache = false;
		//} else if (is_dash_arg_prefix(argv[ix], "nolist", 3)) {
		//	skip_list_exprs = true;
		} else if (is_dash_arg_prefix(argv[ix], "lazy", 3)) {
			lazy = true;
		} else if (is_dash_arg_prefix(argv[ix], "smart", 2)) {
			smart = true;
		} else if (is_dash_arg_colon_prefix(argv[ix], "verbose", &pcolon, 1)) {
			dash_verbose = 1;
			if (pcolon) {
				dash_verbose = atoi(++pcolon);
			}
		} else if (is_dash_arg_prefix(argv[ix], "unittest", 1)) {
			dash_unittest = true;
		} else if (is_dash_arg_colon_prefix(argv[ix], "generate", &pcolon, 3)) {
			generate_some = true;
			if (pcolon) {
				++pcolon;
				if (is_arg_prefix(pcolon, "binary", 1) || is_arg_prefix(pcolon, "pickle", 1)) {
					generate_binary = true;
				//#ifdef WIN32
				//	setmode(fileno(stdout), O_BINARY);
				//#endif
				} else if (is_arg_prefix(pcolon, "wire", 1)) {
					generate_wireline = true;
				}
			}
			generate_fp = stdout;
		} else if (is_dash_arg_colon_prefix(argv[ix], "advertise", &pcolon, 2)) {
			if (pcolon) {
				++pcolon;
				if (is_arg_prefix(pcolon, "binary", 1) || is_arg_prefix(pcolon, "pickle", 1)) {
					advertise_pickled = true;
				} else if (is_arg_prefix(pcolon, "long", 1)) {
					advertise_pickled = false;
				}
			}
			advertise_to = "";
			if (argv[ix+1] && *(argv[ix+1]) != '-') {
				advertise_to = argv[++ix];
			}
		} else if (is_dash_arg_colon_prefix(argv[ix], "query", &pcolon, 2)) {
			if (pcolon) {
				++pcolon;
				if (is_arg_prefix(pcolon, "binary", 1) || is_arg_prefix(pcolon, "pickle", 1)) {
					query_pickled = true;
				} else if (is_arg_prefix(pcolon, "long", 1)) {
					query_pickled = false;
				} else if (is_arg_prefix(pcolon, "jobs", 1)) {
					query_jobs = true;
				}
			}
			query_from = "";
			if (argv[ix+1] && *(argv[ix+1]) != '-') {
				query_from = argv[++ix];
			}
		} else if (is_dash_arg_colon_prefix(argv[ix], "load", &pcolon, 2)) {
			load_from = "";
			if (argv[ix+1] && *(argv[ix+1]) != '-') {
				load_from = argv[++ix];
			}
		} else if (is_dash_arg_colon_prefix(argv[ix], "save", &pcolon, 2)) {
			if (pcolon) {
				++pcolon;
				if (is_arg_prefix(pcolon, "binary", 1) || is_arg_prefix(pcolon, "pickle", 1)) {
					save_pickled = true;
				} else if (is_arg_prefix(pcolon, "hash", 1)) {
					save_hashes = true; save_ads = false;
				} else if (is_arg_prefix(pcolon, "ads", 1)) {
					save_ads = true; save_hashes = false;
				} else if (is_arg_prefix(pcolon, "long", 1)) {
					save_long = true;
				}
			}
			save_to = "";
			if (argv[ix+1] && *(argv[ix+1]) != '-') {
				save_to = argv[++ix];
			}
		} else if (is_dash_arg_colon_prefix(argv[ix], "pickle", &pcolon, 4)) {
			dash_pickle = true;
			pickle_to = "";
			if (argv[ix+1] && *(argv[ix+1]) != '-') {
				pickle_to = argv[++ix];
			}
		} else if (is_dash_arg_colon_prefix(argv[ix], "print", &pcolon, 4)) {
			print_to = "";
			if (argv[ix+1] && *(argv[ix+1]) != '-') {
				print_to = argv[++ix];
			}
		} else if (is_dash_arg_prefix(argv[ix], "dump", 4)) {
			dash_dump = true;
		} else if (is_dash_arg_prefix(argv[ix], "parse", 3)) {
			parse_generated_ads = true;
		} else if (is_dash_arg_prefix(argv[ix], "udp", 3)) {
			use_tcp = false;
		} else if (is_dash_arg_prefix(argv[ix], "tcp", 3)) {
			use_tcp = true;
		} else if (is_dash_arg_prefix(argv[ix], "limit", 3)) {
			if (++ix < argc && *(argv[ix]) != '-') {
				limit = atoi(argv[ix]);
				dash_limit = limit;
			}
		} else if (is_dash_arg_prefix(argv[ix], "seed", 3)) {
			if (++ix < argc && *(argv[ix]) != '-') {
				seed = atoi(argv[ix]);
			}
		} else {
			if (dash_unittest && *(argv[ix]) != '-') {
				testnames.push_back(argv[ix]);
			} else {
				show_usage = true;
			}
			break;
		}
		++ix;
	}

	if ( ! query_from && ! load_from && ! generate_some && ! dash_unittest) { show_usage = true; }

	if (show_usage) {
		fprintf(stderr, "Usage: %s -help\n", argv[0]);
		fprintf(stderr, "       %s -unittest [-verbose] [<test> [<test> ...]]\n", argv[0]);
		fprintf(stderr, "       %s [options] -generate[:binary|wire] | -query <collector> | -query:jobs <schedd> | -load <file>\n", argv[0]);
		fprintf(stderr, "   where [options] are zero or more of:\n");
		fprintf(stderr,
			"  -seed <num>    set seed value for generated ads, default is 42\n"
			"  -save <file>   save the ads to <file> in the form that was queried/generated/loaded\n"
			"  -parse [-cache[:<min_size>] [-smart [-lazy]]]  parse ads with optional minimum string size\n"
			"  -pickle [<file>] pickle the parsed ads and write to <file> if file specified\n"
			"  -print <file>  print the parsed ads to <file>\n"
		);
		exit(0);
	}

	if (dash_unittest) {
		int options = 0;
		return do_unit_tests(stdout, testnames, options);
	}

	if ((advertise_to || query_from) && ! generate_some) {
		config();  // we have to load config in order to connect to a collector

		if (advertise_to) {
			advertise_pool = new DCCollector(advertise_to);

			if (! advertise_pool->locate(Daemon::LOCATE_FOR_LOOKUP)) {
				fprintf(stderr, "Could not locate advertise collector: %s\n", advertise_pool->error());
				exit(1);
			}
		}

		if (query_from) {
			if (query_jobs) {
				query_schedd = new DCSchedd(query_from);

				if ( ! query_schedd->locate(Daemon::LOCATE_FOR_LOOKUP)) {
					fprintf(stderr, "Could not locate query schedd: %s\n", query_schedd->error());
					exit(1);
				}
			} else {
				query_pool = new DCCollector(query_from);

				if (! query_pool->locate(Daemon::LOCATE_FOR_LOOKUP)) {
					fprintf(stderr, "Could not locate query collector: %s\n", query_pool->error());
					exit(1);
				}
			}
		}
	}

	// tell the dprintf code to write to stderr
	dprintf_output_settings my_output;
	my_output.choice = (1<<D_ALWAYS) | (1<<D_ERROR);
	my_output.accepts_all = true;
	my_output.logPath = "2>";
	my_output.HeaderOpts = D_NOHEADER;
	my_output.VerboseCats = 0;
	dprintf_set_outputs(&my_output, 1);

	if (with_cache && cache_string_size >= 0) {
		classad::ClassAdSetExpressionCaching(true, cache_string_size);
	} else {
		classad::ClassAdSetExpressionCaching(with_cache);
	}

	const char * mode = smart ? "smart-no-cache" : "no-cache";
	if (with_cache && classad::ClassAdGetExpressionCaching()) {
		mode = smart ? (lazy ? "smart-lazy-cache" : "smart-cache") : (lazy ? "lazy-cache" : "cache");
	}

	double tmBefore = 0, tmAfter = 0, tmSave = 0, tmParse = 0, tmPickle = 0, tmFinal = 0;
	size_t cbBefore = ProcAPI::getBasicUsage(getpid(), &tmBefore);
	size_t cbAfter = 0, cbSave = 0, cbParse = 0, cbPickle = 0, cbFinal = 0;

	cbAfter = cbBefore; tmAfter = tmBefore;
	
	ClassAdList adlist;
	std::list<std::u8string> ads;
	WireClassadBuffer wab;
	wab.reserve(8192);

	if (query_from) {

		if (query_jobs) {
			fetch_jobs(ads, save_wire_ad /*save_and_check_wire_ad*/, *query_schedd, query_pickled);
		} else {
			fetch_ads(ads, save_wire_ad, *query_pool, query_pickled);
		}

		mode = "WirePair";
		fprintf(stdout, "==== Queried %d Ads, %d Attributes\n", cTotalAds, cTotalAttributes);
		captureAndPrintUsage(mode, "Query", "", tmAfter, cbAfter);

	} else if (load_from) {

		load_ads(ads, save_wire_ad, load_from, mode);

		fprintf(stdout, "==== Loaded %d %s Ads, %d Attributes\n", cTotalAds, mode, cTotalAttributes);
		captureAndPrintUsage(mode, "Load", "", tmAfter, cbAfter);

	} else if (generate_some) {

		generate_ads(ads, wab, save_wire_ad, generate_binary, generate_wireline, seed);

		mode = generate_binary ? "Binary" : (generate_wireline ? "WireLine" : "WirePair");
		fprintf(stdout, "==== Generated %d %s Ads, %d Attributes\n", cTotalAds, mode, cTotalAttributes);
		captureAndPrintUsage(mode, "Gen", "", tmAfter, cbAfter);
	}

	int num_blobs = 0; int num_bytes = 0, max_size=0, min_size=INT_MAX;
	for (auto & bindata : ads) {
		//fprintf(stdout, "%p %x\n", bindata.data(), (int)bindata.size());
		int cb = (int)bindata.size();
		++num_blobs; num_bytes += cb; max_size = MAX(max_size, cb); min_size = MIN(min_size, cb);
	}
	fprintf(stdout, "\t%d %s blobs, using %d total bytes, %d (0x%x) largest, %d (0x%x) smallest\n",
		num_blobs, mode, num_bytes, max_size,max_size, min_size,min_size);

	if (save_to) {
		int fd = safe_create_keep_if_exists(save_to, O_CREAT | O_WRONLY | O_TRUNC | _O_BINARY, 0600);
		if (fd < 0) {
			fprintf(stderr, "could not open %s : %d - %s\n", save_to, errno, strerror(errno));
		} else {
			for (auto & bindata : ads) {
				// if we have been requested to save ClassAds (i.e not hashes),
				// write an ExprStream::ClassAd header before each ad
				if (save_ads && (bindata.front() == classad::ExprStream::EsHash || bindata.front() == classad::ExprStream::EsBigHash)) {
					unsigned char hdr[] = { classad::ExprStream::EsClassAd, 0,0,0,0, 0,0,0,0 };
					unsigned int hash_len = bindata.size();
					memcpy(&hdr[1+sizeof(int)], &hash_len, sizeof(hash_len));
					(void)write (fd, hdr, 9);
				}
				(void)write(fd, bindata.data(), bindata.size());
			}
			close(fd); fd = -1;
		}

		captureAndPrintUsage(mode, "Save", "", tmAfter, cbAfter);
		cbSave = cbAfter; tmSave = tmAfter;
	}

	if (parse_generated_ads) {
		int num_ads = 0, num_attrs = 0;
		const char * cache_mode = with_cache?"via-cache":"without-cache";
		for (auto & bindata : ads) {
			std::string label, comments;
			classad::ExprStream stm(bindata);
			ClassAd * ad = nullptr;
			if (with_cache) {
				if (smart) {
					cache_mode = lazy ? "via-cache (smart+lazy)" : "via-cache (smart)";
					ad = smartMakeClassAd(stm, lazy, &label, &comments);
				} else {
					ad = ClassAd::MakeViaCache(stm, &label, &comments);
				}
			} else {
				ad = ClassAd::Make(stm, &label);
			}
			//fprintf(stdout, "parsed ad %s %s to %p\n", cache_mode, label.c_str(), ad);
			if (ad) {
				++num_ads;
				num_attrs += ad->size();
				adlist.Insert(ad);
			}
		}

		captureAndPrintUsage(mode, "Parse", "", tmAfter, cbAfter);
		cbParse = cbAfter; tmParse = tmAfter;
		fprintf(stdout, "\tparsed %d %s blobs %s to %d classads and %d total attributes\n",
			num_blobs, mode, cache_mode, num_ads, num_attrs);

		std::list<std::u8string> ads2;
		if (pickle_to) {
			classad::ExprStreamMaker maker;
			maker.grow(max_size);
			std::string label;
			int adId = 0;
			int ad2_min = INT_MAX, ad2_max = 0, ad2_tot = 0;

			adlist.Open();
			ClassAd * ad = nullptr;
			while ((ad = adlist.Next())) {
				maker.clear();
				formatstr(label, "Ad%d", ++adId);
				ad->Pickle(maker, label, nullptr, false);

				ad2_min = MIN(ad2_min, maker.size());
				ad2_max = MAX(ad2_max, maker.size());
				ad2_tot += maker.size();

				ads2.emplace_back((const char8_t *)maker.data(), maker.size());
			}
			adlist.Close();

			captureAndPrintUsage("ParsedAd", "Pickle", "", tmAfter, cbAfter);
			cbPickle = cbAfter; tmPickle = tmAfter;
			fprintf(stdout, "\tpickled %d ClassAds to blobs totaling %.4f Mbytes (min=%d, max=%d)\n",
				adId, ad2_tot/(1024*1024.0), ad2_min, ad2_max);

			if (*pickle_to) {
				int fd = safe_create_keep_if_exists(pickle_to, O_CREAT | O_WRONLY | O_TRUNC | _O_BINARY, 0600);
				if (fd < 0) {
					fprintf(stderr, "could not open %s : %d - %s\n", pickle_to, errno, strerror(errno));
				} else {
					for (auto & bindata : ads2) {
						(void)write(fd, bindata.data(), bindata.size());
					}
					close(fd); fd = -1;
				}

				captureAndPrintUsage("Pickled ", "Save", "", tmAfter, cbAfter);
			}
		}
	}

	if (print_to && (adlist.Length() > 0)) {

		int fd = safe_create_keep_if_exists(print_to, O_CREAT | O_WRONLY | O_TRUNC | _O_BINARY, 0600);
		if (fd < 0) {
			fprintf(stderr, "could not open %s : %d - %s\n", print_to, errno, strerror(errno));
		} else {
			std::string buffer;

			adlist.Open();
			ClassAd * ad = nullptr;
			while ((ad = adlist.Next())) {
				if ( ! buffer.empty()) { buffer = "\n"; }
				sPrintAdWithSecrets(buffer, *ad);
				write(fd, buffer.data(), buffer.size());
			}
			adlist.Close();
			close(fd); fd = -1;
		}

		captureAndPrintUsage("ParsedAd", "Print", "", tmAfter, cbAfter);
	}

	if (advertise_to) {
		fprintf(stderr, "-advertise not implemented\n");
	}

	classad::CachedExprEnvelope::_debug_print_stats(stdout);

	ads.clear();
	adlist.Clear();

	captureAndPrintUsage(mode, "Delete", "", tmAfter, cbAfter);
	cbFinal = cbAfter; tmFinal = tmAfter;

	return rval;
}

void dump_hex(FILE* out,
	const char * label, const char * indent,
	const binary_span & dat)
{
	char hbuffer[16*3 + 1];
	char cbuffer[16+1];
	const char * p = (const char*)dat.data();
	const char * endp = p + dat.size();
	for ( ; p < endp; p += 16) {
		size_t len = MIN(16, endp - p);
		memset(hbuffer, ' ', sizeof(hbuffer)); hbuffer[sizeof(hbuffer-1)] = 0;
		debug_hex_dump(hbuffer, p, len);
		memset(cbuffer, 0, sizeof(cbuffer));
		for (size_t ix = 0; ix < len; ++ix) {
			unsigned char ch = p[ix];
			if (ch < 32) ch = '.';
			else if (ch > 127) {
				if (ch == classad::ExprStream::EsElvisOp || ch == classad::ExprStream::EsTernaryOp) { ch = '?'; }
				else { ch = "#$**%%_@"[(ch>>4)&7]; }
			}
			cbuffer[ix] = ch;
		}
		fprintf(out, "%10s\t%-48s %s\n", label, hbuffer, cbuffer);
		label = indent;
	}
}

int pickle_unit_test(FILE* out, int options)
{
	constexpr const char * exprs[]{
		// reserved words
		"true", "false", "TRUE", "FALSE", "True", "False", "tRuE", "fAlSe",
		"undefined", "UNDEFINED", "Undefined", "uNdEfInEd",
		"error", "ERROR", "Error", "eRrOr",
		// numbers
		"0", "1", "2", "10", "9999", "0111", "-1", "+1",
		"127", "128", "255", "256", "32767", "32768", "65535", "12345678910",
		"-127", "-128", "-255", "-256", "-257", "-32768", "-32769", "-65535", "-12345678910",
		"0.0", "1.0", ".0", ".1", "000.1", ".0003",  "1e0", "1e-3", // these don't parse: "+0.", "-0.",
		// strings
		"\"\"", "\" \"", "\",\"", "\"abcdefg\"", "\"a b c d e\"",
		// attrs
		"A", "ab", "A9", "_CONDOR_", "A.B", "A.B.C", "AAA.B", "MY.BB", "target.yy", "self.name",
		// long attrs
		"aaaaaaaaa0bbbbbbbbb1ccccccccc2ddddddddd3eeeeeeeee4fffffffff5ggggggggg6hhhhhhhhh7iiiiiiiii8jjjjjjjjj9"
		"kkkkkkkkk0lllllllll1mmmmmmmmm2nnnnnnnnn3ooooooooo4ppppppppp5qqqqqqqqq6rrrrrrrrr7sssssssss8ttttttttt9"
		"uuuuuuuuu0vvvvvvvvv1wwwwwwwww2xxxxxxxxx3yyyyyyyyy4zzzzzzzzz5",
		"aaaaaaaaa0bbbbbbbbb1ccccccccc2ddddddddd3eeeeeeeee4fffffffff5ggggggggg6hhhhhhhhh7iiiiiiiii8jjjjjjjjj9"
		"kkkkkkkkk0lllllllll1mmmmmmmmm2nnnnnnnnn3ooooooooo4ppppppppp5qqqqqqqqq6rrrrrrrrr7sssssssss8ttttttttt9"
		"uuuuuuuuu0vvvvvvvvv1wwwwwwwww2xxxxxxxxx3yyyyyyyyy4zzzzzz",
		"aaaaaaaaa0bbbbbbbbb1ccccccccc2ddddddddd3eeeeeeeee4fffffffff5ggggggggg6hhhhhhhhh7iiiiiiiii8jjjjjjjjj9"
		"kkkkkkkkk0lllllllll1mmmmmmmmm2nnnnnnnnn3ooooooooo4ppppppppp5qqqqqqqqq6rrrrrrrrr7sssssssss8ttttttttt9"
		"uuuuuuuuu0vvvvvvvvv1wwwwwwwww2xxxxxxxxx3yyyyyyyyy4zzzzz",
		// math
		"1 + 2", "2 * 3", "44*55", "6-7", "8/9", "1/1",
		// bitwise
		"~9", "a|B", "C&D", "1<<2", "128>>3",
		// comparison
		"A>b", "b<A", "c==d", "e!=f", "g=?=h", "h is J", "j =!= k", "k isnt undefined",
		// ternary
		"A ? B : C", "AA?1:2",
		// elvis
		"A ?: B", "A ? B : C", "A? :B", "A?B:C",
		"A?:B[0]", "A[0]?:B[1]", "(A?:B)[0]",
		"A.B?:C", ".A?:.B", "MY.A?:\"\"",
		"A?:false is undefined", "(A?:false) is undefined",
		"A?:0 is B?:0", "MY.A?:Y.C is MY.B?:Z.D",
		"A?:D.XX is B?:C.YY", "(A?:D).XX is B?:C.YY",
		// functions
		"time()", "strcat(\"a\",\"b\")", "join(\",\", a, b, c)",
		// lists
		"{}", "{1}", "{a,b}", "{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}",
		// ads
		"[]", "[a=-1;]", "[a=1;b=2;]", "[a=\"\";].a",
		"[aaaaaaaaa0bbbbbbbbb1ccccccccc2ddddddddd3eeeeeeeee4fffffffff5ggggggggg6hhhhhhhhh7iiiiiiiii8jjjjjjjjj9"
		"kkkkkkkkk0lllllllll1mmmmmmmmm2nnnnnnnnn3ooooooooo4ppppppppp5qqqqqqqqq6rrrrrrrrr7sssssssss8ttttttttt9"
		"uuuuuuuuu0vvvvvvvvv1wwwwwwwww2xxxxxxxxx3yyyyyyyyy4zzzzzzzzz5=0;"
		"aaaaaaaaa0bbbbbbbbb1ccccccccc2ddddddddd3eeeeeeeee4fffffffff5ggggggggg6hhhhhhhhh7iiiiiiiii8jjjjjjjjj9"
		"kkkkkkkkk0lllllllll1mmmmmmmmm2nnnnnnnnn3ooooooooo4ppppppppp5qqqqqqqqq6rrrrrrrrr7sssssssss8ttttttttt9"
		"uuuuuuuuu0vvvvvvvvv1wwwwwwwww2xxxxxxxxx3yyyyyyyyy4zzzzzz=1;]",

	};

	int num_ok = 0, num_failed = 0;

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd(true,true);
	classad::ExprStreamMaker maker;
	std::string unparsed_expr, unparsed_copy;

	for (auto & exprstr : exprs) {
		if (dash_verbose) fprintf(out, "\n");
		ExprTree * tree = nullptr;
		if ( ! parser.ParseExpression(exprstr, tree, true)) {
			fprintf(out, "\tparse failed : %s\n", exprstr);
			++num_failed;
			continue;
		}

		maker.clear();
		tree->Pickle(maker);
		if (dash_verbose) dump_hex(out, exprstr, "\t", maker.dataspan());
		
		int err = 0;
		classad::ExprStream stm(maker.dataspan());
		ExprTree * tree2 = classad::ExprTree::Make(stm, false, &err);
		if ( ! tree2 || err) {
			fprintf(out, "\tunpickle failed : %d\n", err);
			++num_failed;
			continue;
		} else if (*tree != *tree2) {
			fprintf(out, "\tunpickle failed : %p != %p\n", tree, tree2);
			++num_failed;
			continue;
		}

		unparsed_expr.clear();
		unparser.Unparse(unparsed_expr, tree);
		unparsed_copy.clear();
		unparser.Unparse(unparsed_copy, tree2);
		if (unparsed_expr != unparsed_copy) {
			fprintf(out, "\tunpickle did not round-trip: '%s' != '%s'\n",
				unparsed_expr.c_str(), unparsed_copy.c_str());
			++num_failed;
			continue;
		}
		if (dash_verbose) fprintf(out, "\t Success : '%s' == '%s'\n", unparsed_expr.c_str(), unparsed_copy.c_str());
		++num_ok;
	}

	return num_failed ? 1 : 0;
}

static const char * getKindName(unsigned char kind)
{
	static const char * akind[]{
		"ATTRREF",
		"OP",
		"FN_CALL",
		"CLASSAD",
		"EXPR_LIST",
		"ENVELOPE",
		"ERROR_LITERAL",
		"UNDEFINED",
		"BOOLEAN",
		"INTEGER",
		"REAL",
		"RELTIME",
		"ABSTIME",
		"STRING",
		""
	};
	return akind[MIN(kind, COUNTOF(akind))];
}

static FILE* test_outfile = nullptr;

static int num_lookup_failures = 0;
static bool test_lookup_on_wab(std::list<std::u8string> & /*ads*/, WireClassadBuffer &wab, int id)
{
	FILE* out = test_outfile;

	classad::ExprStream stmAd = wab.stream();

	ClassAd ad;
	std::string title;
	// build_index will invalidate the stmHash and label, so we scope them.
	{
		std::string_view label;
		classad::ExprStream stmhash = wab.find_hash(stmAd, label);
		ad.Update(stmhash);
		title = label;
	}

	if (dash_verbose > 1) { putc('\n', out); }

	int cFail=0, cAttrs=0;
	wab.build_index();
	for (auto const &[attr, expr] : ad) {
		++cAttrs;
		classad::ExprStream stmExpr;
		if ( ! wab.Lookup(attr.c_str(), &stmExpr)) {
			fprintf(out, "lookup of %s failed\n", attr.c_str());
			++cFail;
		} else {
			classad::ExprTree::NodeKind kind;
			unsigned char ct = stmExpr.peekKind(kind);
			switch (kind) {
			case classad::ExprTree::NodeKind::BOOLEAN_LITERAL: {
				bool bval = false, bvalAd = false;
				  if ( ! classad::Value::Get(stmExpr, bval) || ! ExprTreeIsLiteralBool(expr, bvalAd) || (bval != bvalAd)) {
				    fprintf(out, "%s : (%x,%d) ERROR %s != %s\n", attr.c_str(), ct, kind, bval ? "true" : "false", bvalAd ? "true" : "false");
				  } else {
					if (dash_verbose > 1) fprintf(out, "%s : (%x,%d) %s\n", attr.c_str(), ct, kind, bval ? "true" : "false");
				  }
				} break;
			case classad::ExprTree::NodeKind::INTEGER_LITERAL: {
				long long lval=-42, lvalAd=-41;
				  if ( ! classad::Value::Get(stmExpr, lval) || ! ExprTreeIsLiteralNumber(expr, lvalAd) || (lval != lvalAd)) {
					fprintf(out, "%s : (%x,%d) ERROR %lld != %lld\n", attr.c_str(), ct, kind, lval, lvalAd);
				  } else {
					if (dash_verbose > 1) fprintf(out, "%s : (%x,%d) %lld\n", attr.c_str(), ct, kind, lval);
				  }
				} break;
			case classad::ExprTree::NodeKind::REAL_LITERAL: {
				double dval = -42.42, dvalAd = -41.41;
				  if ( ! classad::Value::Get(stmExpr, dval) || ! ExprTreeIsLiteralNumber(expr, dvalAd) || (dval != dvalAd)) {
					fprintf(out, "%s : (%x,%d) ERROR %f != %f\n", attr.c_str(), ct, kind, dval, dvalAd);
				  } else {
					if (dash_verbose > 1) fprintf(out, "%s : (%x,%d) %f\n", attr.c_str(), ct, kind, dval);
				  }
				} break;
			case classad::ExprTree::NodeKind::STRING_LITERAL: {
				std::string_view sval;
				std::string str, strAd;
				  if ( ! classad::Value::Get(stmExpr, sval) || ! ExprTreeIsLiteralString(expr, strAd) || (sval != strAd)) {
					str = sval;
					fprintf(out, "%s : (%x,%d) ERROR \"%s\" != \"%s\"\n", attr.c_str(), ct, kind, str.c_str(), strAd.c_str());
				  } else {
					str = sval;
					if (dash_verbose > 1) fprintf(out, "%s : (%x,%d) \"%s\"\n", attr.c_str(), ct, kind, str.c_str());
				  }
			} break;
			default:
				if (dash_verbose > 1) fprintf(out, "%s : (%x,%d) %s %d bytes \n", attr.c_str(), ct, kind, getKindName(kind), (int)stmExpr.size());
				break;
			}
		}
	}

	if (dash_verbose) {
		if (cFail) {
			fprintf(out, "# %s %d attributes : %d failed lookups\n", title.c_str(), cAttrs, cFail);
		} else {
			fprintf(out, "# %s %d attributes\n", title.c_str(), cAttrs);
		}
	}

	num_lookup_failures += cFail;

	return true;
}

int lookup_unit_test(FILE* out, int options)
{
	test_outfile = out;
	if (dash_verbose) { putc('\n', out); }

	std::list<std::u8string> dummy;
	WireClassadBuffer wab;
	const bool pickled = dash_pickle;
	generate_ads(dummy, wab, test_lookup_on_wab, pickled, false, 42, dash_limit);

	return num_lookup_failures ? 1 : 0;
}

int num_updateadfromraw_failures = 0;
static bool test_updatead_from_raw(std::list<std::u8string> & /*ads*/, WireClassadBuffer &wab, int id)
{
	FILE* out = test_outfile;
	int cAttrs = 0;
	bool update_fail = false, count_fail = false;

	// if (dash_verbose > 1) { putc('\n', out); }

	int options = 0;
	time_t time_and_flags = wab.get_time_and_flags();
	auto stmAd = wab.stream();

	ClassAd ad;
	if ( ! updateAdFromRaw(ad, time_and_flags, stmAd, options, nullptr)) {
		update_fail = true;
	}

	if (dash_verbose > 1) {
		fPrintAd(out, ad, false);
	}

	std::string title;
	{
		stmAd = wab.stream();
		std::string_view label;
		auto stmHash = wab.find_hash(stmAd, label);
		cAttrs = wab.read_hash(stmHash);
		if (wab.is_binary() && wab.get_servertime()) { cAttrs += 1;} // account for injection of ServerTime attribute
		title = label;
	}

	if ( ! cAttrs || cAttrs != ad.size()) {
		count_fail = true;
	}
	if (dash_verbose) {
		if (update_fail || count_fail) {
			fprintf(out, "# %s %d attributes : %s %s %d=%d\n", title.c_str(), cAttrs,
				update_fail ? "update failed" : "", count_fail ? "wrong count" : "", cAttrs, (int)ad.size());
		} else {
			fprintf(out, "# %s %d attributes\n", title.c_str(), cAttrs);
		}
	}

	num_updateadfromraw_failures += (update_fail || count_fail);
	return true;
}

int updateadfromraw_unit_test(FILE* out, int options)
{
	test_outfile = out;
	if (dash_verbose) { putc('\n', out); }

	std::list<std::u8string> ads;
	WireClassadBuffer wab;
	const bool pickled = dash_pickle;
	generate_ads(ads, wab, test_updatead_from_raw, pickled, false, 42, dash_limit);

	return num_updateadfromraw_failures ? 1 : 0;
}

int num_projectadfromraw_failures = 0;
classad::References * projectadfromraw_projection = nullptr;
int expected_min_attrs_after_projection = 0;
static bool test_projectad_from_raw(std::list<std::u8string> & /*ads*/, WireClassadBuffer &wab, int id)
{
	FILE* out = test_outfile;
	int cMaxAttrs = 0, cMinAttrs = expected_min_attrs_after_projection;
	bool update_fail = false, count_fail = false;

	// if (dash_verbose > 1) { putc('\n', out); }

	int options = 0;
	time_t time_and_flags = wab.get_time_and_flags();
	auto stmAd = wab.stream();

	ClassAd ad;
	if ( ! updateAdFromRaw(ad, time_and_flags, stmAd, options, projectadfromraw_projection)) {
		update_fail = true;
	}

	if (dash_verbose > 1) {
		fPrintAd(out, ad, false);
	}

	std::string title;
	{
		stmAd = wab.stream();
		std::string_view label;
		auto stmHash = wab.find_hash(stmAd, label);
		title = label;
	}

	// TODO: handle the case where the projection
	cMaxAttrs = projectadfromraw_projection->size();
	if (ad.size() > cMaxAttrs || ad.size() < cMinAttrs) {
		count_fail = true;
	}
	if (dash_verbose) {
		if (update_fail || count_fail) {
			fprintf(out, "# %s of %d attributes : %s %s %d>%d<%d\n", title.c_str(), cMaxAttrs,
				update_fail ? "update failed" : "",
				count_fail ? "wrong count" : "",
				cMinAttrs, (int)ad.size(), cMaxAttrs);
		} else {
			fprintf(out, "# %s %d attributes\n", title.c_str(), (int)ad.size());
		}
	}

	num_projectadfromraw_failures += (update_fail || count_fail);
	return true;
}

int projectadfromraw_unit_test(FILE* out, int options)
{
	test_outfile = out;
	if (dash_verbose) { putc('\n', out); }

	// TODO: fix check to handle the case when an attribute is expectedly missing from the projection. 

	classad::References proj;
	projectadfromraw_projection = &proj;
	proj.insert(ATTR_MY_TYPE);
	proj.insert(ATTR_NAME);
	proj.insert(ATTR_MACHINE);
	proj.insert(ATTR_MY_ADDRESS);
	proj.insert(ATTR_CPUS);
	proj.insert(ATTR_MEMORY);
	proj.insert(ATTR_CONDOR_LOAD_AVG);
	proj.insert(ATTR_STATE);

	std::list<std::u8string> ads;
	WireClassadBuffer wab;
	const bool pickled = dash_pickle;
	generate_ads(ads, wab, test_projectad_from_raw, pickled, false, 42, dash_limit);

	projectadfromraw_projection = nullptr;
	return num_projectadfromraw_failures ? 1 : 0;
}

static const struct {
	const char * name;
	const char * descr;
	int (*func)(FILE* out, int options);
} test_table[] = {
	{ "pickle",           "Test round/trip pickle/unpickle of expressions", pickle_unit_test },
	{ "lookup",           "Test WireClassAdBuffer::lookup on a set of ads", lookup_unit_test },
	{ "updateadfromraw",  "Test updateAdFromRaw on a set of ads", updateadfromraw_unit_test },
	{ "projectadfromraw", "Test updateAdFromRaw with projection on a set of ads", projectadfromraw_unit_test },
};

int do_unit_tests(FILE* out, std::vector<const char *> & tests, int options)
{
	int num_ok = 0, num_failed = 0;

	int ix = 0;
	std::map<const char *, int, classad::CaseIgnLTStr> all_tests;
	for (auto & entry : test_table) { all_tests[entry.name] = ix++; }

	// if no test names passed, run all tests
	if (tests.empty()) for (auto & entry : test_table) tests.push_back(entry.name);

	fprintf(out, "Running %s unit tests...\n", dash_pickle ? "pickled wire form" : "long wire form");

	double dstart = _condor_debug_get_time_double();
	for (auto * test : tests) {
		auto found = all_tests.find(test);
		if (found != all_tests.end()) {
			const char * testname = found->first;
			fprintf(out,
				"\nTesting : %s (%s)"
				"\n          ",
				testname, test_table[found->second].descr);
			int result = test_table[found->second].func(out, options);
			double dend = _condor_debug_get_time_double();
			if (result != 0) {
				++num_failed;
				fprintf(out, "%.3f sec ---------- %s : %d FAILED\n", dend-dstart, testname, result);
			} else {
				++num_ok;
				fprintf(out, "%.3f sec ---------- %s : ok\n", dend-dstart, testname);
			}
			dstart = dend;
		}
	}

	if (num_failed) {
		fprintf(out, "\nTest results: %d ok %d FAILED\n", num_ok, num_failed);
	} else {
		fprintf(out, "\nTest results: %d ok\n", num_ok);
	}
	return num_failed;
}
