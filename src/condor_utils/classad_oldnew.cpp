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
#include "stream.h"
#include "reli_sock.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "my_hostname.h"

#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "compat_classad.h"

// local helper functions, options are one or more of PUT_CLASSAD_* flags
int _putClassAd(Stream *sock, const classad::ClassAd& ad, int options,
	const classad::References *encrypted_attrs);
int _putClassAd(Stream *sock, const classad::ClassAd& ad, int options,
	const classad::References &whitelist, const classad::References *encrypted_attrs);


static const char *SECRET_MARKER = "ZKM"; // "it's a Zecret Klassad, Mon!"

bool getClassAd( Stream *sock, classad::ClassAd& ad )
{
	int 					numExprs;
	std::string				inputLine;

	ad.Clear( );

	sock->decode( );
	if( !sock->code( numExprs ) ) {
		dprintf(D_FULLDEBUG, "FAILED to get number of expressions.\n");
 		return false;
	}

	// at least numExprs are coming, but we may add
	// my, target, and a couple extra right away

	ad.rehash(numExprs + 5);

		// pack exprs into classad
	for( int i = 0 ; i < numExprs ; i++ ) {
		char const *strptr = NULL;
		if( !sock->get_string_ptr( strptr ) || !strptr ) {
			dprintf(D_FULLDEBUG, "FAILED to get expression string.\n");
			return( false );
		}

		bool inserted = false;
		if(strcmp(strptr,SECRET_MARKER) ==0 ){
			char *secret_line = NULL;
			if( !sock->get_secret(secret_line) ) {
				dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
				break;
			}
			inserted = InsertLongFormAttrValue(ad, secret_line, true);
			free( secret_line );
		}
		else {
			inserted = InsertLongFormAttrValue(ad, strptr, true);
		}

		// inserts expression at a time
		if ( ! inserted)
		{
			dprintf(D_FULLDEBUG, "FAILED to insert %s\n", strptr );
			return false;
		}
		
	}

	// We fetch but ignore the special MyType and TargetType fields.
	// These attributes, if defined, are included in the regular set
	// of name/value pairs.
	if (!sock->get(inputLine)) {
		 dprintf(D_FULLDEBUG, "FAILED to get(inputLine)\n" );
		return false;
	}

	if (!sock->get(inputLine)) {
		 dprintf(D_FULLDEBUG, "FAILED to get(inputLine) 2\n" );
		return false;
	}

	return true;
}


//uncomment this to enable runtime profiling of getClassAdEx, be aware that libcondorapi will have link errors with the profiling code.
//#define PROFILE_GETCLASSAD
#ifdef PROFILE_GETCLASSAD
  #include "generic_stats.h"
  stats_entry_probe<double> getClassAdEx_runtime;
  stats_entry_probe<double> getClassAdExCache_runtime;
  stats_entry_probe<double> getClassAdExCacheLazy_runtime;
  stats_entry_probe<double> getClassAdExParse_runtime;
  stats_entry_probe<double> getClassAdExLiteral_runtime;
  stats_entry_probe<double> getClassAdExLiteralBool_runtime;
  stats_entry_probe<double> getClassAdExLiteralNumber_runtime;
  stats_entry_probe<double> getClassAdExLiteralString_runtime;
  #define IF_PROFILE_GETCLASSAD(a) a;

  void getClassAdEx_addProfileStatsToPool(StatisticsPool * pool, int publevel)
  {
	if ( ! pool) return;

	#define ADD_PROBE(name) pool->AddProbe(#name, &name##_runtime, #name, publevel | IF_RT_SUM);
	ADD_PROBE(getClassAdEx);
	ADD_PROBE(getClassAdExCache);
	ADD_PROBE(getClassAdExCacheLazy);
	ADD_PROBE(getClassAdExParse);
	ADD_PROBE(getClassAdExLiteral);
	ADD_PROBE(getClassAdExLiteralBool);
	ADD_PROBE(getClassAdExLiteralNumber);
	ADD_PROBE(getClassAdExLiteralString);
	#undef ADD_PROBE
  }

  void getClassAdEx_clearProfileStats()
  {
	getClassAdEx_runtime.Clear();
	getClassAdExCache_runtime.Clear();
	getClassAdExCacheLazy_runtime.Clear();
	getClassAdExParse_runtime.Clear();
	getClassAdExLiteral_runtime.Clear();
	getClassAdExLiteralBool_runtime.Clear();
	getClassAdExLiteralNumber_runtime.Clear();
	getClassAdExLiteralString_runtime.Clear();
  }

#else
  #define IF_PROFILE_GETCLASSAD(a)
  void getClassAdEx_addProfileStatsToPool(StatisticsPool *, int) {}
  void getClassAdEx_clearProfileStats() {}
#endif

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


bool getClassAdEx( Stream *sock, classad::ClassAd& ad, int options)
{
	int cb;
	const char *strptr;
	std::string attr;
	bool use_cache = (options & GET_CLASSAD_NO_CACHE) == 0;
	bool cache_lazy = (options & GET_CLASSAD_LAZY_PARSE) != 0;
	bool fast_tricks = (options & GET_CLASSAD_FAST) != 0;
	const size_t always_cache_string_size = 128; // no fast parse for strings > this size.

#ifdef PROFILE_GETCLASSAD
	_condor_auto_accum_runtime< stats_entry_probe<double> > rt(getClassAdEx_runtime);
	double rt_last = rt.begin;
#endif

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);

	if ( ! (options & GET_CLASSAD_NO_CLEAR)) {
		ad.Clear( );
	}

	sock->decode( );

	int numExprs;
	if( !sock->code( numExprs ) ) {
		return false;
	}

	// at least numExprs are coming, but we may add
	// my, target, and a couple extra right away
	// Auth (id,method) update(total,seq,lost,history)

	if ( ! (options & GET_CLASSAD_NO_CLEAR)) {
		ad.rehash(numExprs + 2 + 7);
	}

		// pack exprs into classad
	for (int ii = 0 ; ii < numExprs ; ++ii) {
		strptr = NULL;
		if ( ! sock->get_string_ptr(strptr, cb) || ! strptr) {
			return false;
		}

		bool its_a_secret = false;
		if ((*strptr=='Z') && strptr[1] == 'K' && strptr[2] == 'M' && strptr[3] == 0) {
			its_a_secret = true;
			if ( ! sock->get_secret(strptr, cb) || ! strptr) {
				dprintf(D_FULLDEBUG, "getClassAd Failed to read encrypted ClassAd expression.\n");
				break;
			}
			// cb includes the terminating NUL character.
			// TODO This strlen() should be unnecessary. Once we're confident
			//   that is form of get_secret() isn't buggy, the strlen()
			//   and size check should be removed.
			int cch = strlen(strptr);
			if (cch != cb-1) {
				dprintf(D_FULLDEBUG, "getClassAd get_secret returned %d for string with 0 at %d\n", cb, cch);
			}
		}

		// this splits at the =, puts the attribute name into attr
		// and returns a pointer to the first non-whitespace character after the =
		const char * rhs;
		if ( ! SplitLongFormAttrValue(strptr, attr, rhs)) {
			dprintf(D_ALWAYS, "getClassAd FAILED to split%s %s\n", its_a_secret?" secret":"", strptr );
			return false;
		}

		// Fast tricks pre-parses the right hand side when it is detected as a simple literal
		// this is faster than letting the classad parser parse it (as of 8.7.0) and also
		// uses less memory than letting the classad cache see it since literal nodes are the same
		// size as envelope nodes.
		//
		bool inserted = false;
		IF_PROFILE_GETCLASSAD(int subtype = 0);
		size_t cbrhs = cb - (rhs - strptr);
		if (fast_tricks) {
			char ch = rhs[0];
			if (cbrhs == 5 && (ch&~0x20) == 'T' && (rhs[1]&~0x20) == 'R' && (rhs[2]&~0x20) == 'U' && (rhs[3]&~0x20) == 'E') {
				inserted = ad.InsertLiteral(attr, classad::Literal::MakeBool(true));
				IF_PROFILE_GETCLASSAD(subtype = 1);
			} else if (cbrhs == 6 && (ch&~0x20) == 'F' && (rhs[1]&~0x20) == 'A' && (rhs[2]&~0x20) == 'L' && (rhs[3]&~0x20) == 'S' && (rhs[4]&~0x20) == 'E') {
				inserted = ad.InsertLiteral(attr, classad::Literal::MakeBool(false));
				IF_PROFILE_GETCLASSAD(subtype = 1);
			} else if (cbrhs < 30 && (ch == '-' || (ch >= '0' && ch <= '9'))) {
				if (strchr(rhs, '.')) {
					char *pe = NULL;
					double d = strtod(rhs, &pe);
					if (*pe == 0 || *pe == '\r' || *pe == '\n') {
						inserted = ad.InsertLiteral(attr, classad::Literal::MakeReal(d));
						IF_PROFILE_GETCLASSAD(subtype = 2);
					}
				} else {
					const char * pe = NULL;
					long long ll = myatoll(rhs, pe);
					if (*pe == 0 || *pe == '\r' || *pe == '\n') {
						inserted = ad.InsertLiteral(attr, classad::Literal::MakeInteger(ll));
						IF_PROFILE_GETCLASSAD(subtype = 2);
					}
				}
			} else if (cbrhs < always_cache_string_size && ch == '"') { // 128 because we want long strings in the cache.
				size_t cch = IsSimpleString(rhs);
				if (cch) {
					inserted = ad.InsertLiteral(attr, classad::Literal::MakeString(rhs+1, cch-2));
					IF_PROFILE_GETCLASSAD(subtype = 3);
				}
			}
		}

		if (inserted) {
		#ifdef PROFILE_GETCLASSAD
			double dt = rt.tick(rt_last);
			getClassAdExLiteral_runtime.Add(dt);
			switch (subtype) {
			case 1: getClassAdExLiteralBool_runtime.Add(dt); break;
			case 2: getClassAdExLiteralNumber_runtime.Add(dt); break;
			case 3: getClassAdExLiteralString_runtime.Add(dt); break;
			}
		#endif
		} else {
			// we can't cache nested classads or lists, so just parse and insert them
			bool cache = use_cache && (*rhs != '[' && *rhs != '{');
			if (cache) {
				if (cache_lazy) {
					inserted = ad.InsertViaCache(attr, rhs, true);
					IF_PROFILE_GETCLASSAD(getClassAdExCacheLazy_runtime.Add(rt.tick(rt_last)));
				} else {
					inserted = ad.InsertViaCache(attr, rhs, false);
					IF_PROFILE_GETCLASSAD(getClassAdExCache_runtime.Add(rt.tick(rt_last)));
				}
			} else {
				ExprTree *tree = parser.ParseExpression(rhs);
				if (tree) {
					inserted = ad.Insert(attr, tree);
				}
				IF_PROFILE_GETCLASSAD(getClassAdExParse_runtime.Add(rt.tick(rt_last)));
			}
		}
		if ( ! inserted) {
			dprintf(D_ALWAYS, "getClassAd FAILED to insert%s %s\n", its_a_secret?" secret":"", strptr );
			return false;
		}
	}

	if (options & GET_CLASSAD_NO_TYPES) {
		return true;
	}

		// get type info
	if (!sock->get_string_ptr(strptr, cb)) {
		dprintf(D_FULLDEBUG, "getClassAd FAILED to get MyType\n" );
		return false;
	}
#if 0 // we fetch but ignore MyType and TargetType
	if (strptr && strptr[0]) {
		if (YourString(strptr) != "(unknown type)" && !ad.InsertAttr("MyType",strptr)) {
			dprintf(D_FULLDEBUG, "getClassAd FAILED to insert MyType=\"%s\"\n", strptr );
			return false;
		}
	}
#endif

	if (!sock->get_string_ptr(strptr, cb)) {
		dprintf(D_FULLDEBUG, "getClassAd FAILED to get TargetType\n" );
		return false;
	}
#if 0 // we fetch but ignore MyType and TargetType
	if (strptr && strptr[0]) {
		if (YourString(strptr) != "(unknown type)" && !ad.InsertAttr("TargetType",strptr)) {
			dprintf(D_FULLDEBUG, "getClassAd FAILED to insert TargetType=\"%s\"\n", strptr );
			return false;
		}
	}
#endif

	return true;
}


int getClassAdNonblocking( ReliSock *sock, classad::ClassAd& ad )
{
	int retval;
	bool read_would_block;
	{
		BlockingModeGuard guard(sock, true);
		retval = getClassAd(sock, ad);
		read_would_block = sock->clear_read_block_flag();
	}
	if (!retval) {
		return 0;
	} else if (read_would_block) {
		return 2;
	}
	return retval;
}

bool
getClassAdNoTypes( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 					numExprs = 0; // Initialization clears Coverity warning
	std::string					buffer;
	classad::ClassAd		*upd=NULL;
	std::string				inputLine;

	parser.SetOldClassAd( true );

	ad.Clear( );

	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

		// pack exprs into classad
	buffer = "[";
	for( int i = 0 ; i < numExprs ; i++ ) {
		if( !sock->get( inputLine ) ) { 
			return( false );	 
		}		

        if(strcmp(inputLine.c_str(),SECRET_MARKER) ==0 ){
            char *secret_line = NULL;
            if( !sock->get_secret(secret_line) ) {
                dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
                break;
            }
	    inputLine = secret_line;
	    free( secret_line );
        }

		buffer += inputLine + ";";
	}
	buffer += "]";

		// parse ad
	if( !( upd = parser.ParseClassAd( buffer ) ) ) {
		return( false );
	}

		// put exprs into ad
	ad.Update( *upd );
	delete upd;

	return true;
}

// read an attribute from a query ad, and turn it into a classad projection (i.e. a set of attributes)
// the projection attribute can be a string, or (if allow_list is true) a classad list of strings.
// returns:
//    -1 if projection does not evaluate
//    -2 if projection does not convert to string
//    0  if no projection or projection is valid but empty
//    1  the projection is non-empty
//
int mergeProjectionFromQueryAd(classad::ClassAd & queryAd, const char * attr_projection, classad::References & projection, bool allow_list /*= false*/)
{
	if ( ! queryAd.Lookup(attr_projection))
		return 0; // no projection

	classad::Value value;
	if ( ! queryAd.EvaluateAttr(attr_projection, value)) {
		return -1;
	}

	classad::ExprList *list = NULL;
	if (allow_list && value.IsListValue(list)) {
		for (auto it : *list) {
			std::string attr;
			if (!ExprTreeIsLiteralString(it, attr)) {
				return -2;
			}
			projection.insert(attr);
		}
		return projection.empty() ? 0 : 1;
	}

	std::string proj_list;
	if (value.IsStringValue(proj_list)) {
		StringTokenIterator list(proj_list);
		const std::string * attr;
		while ((attr = list.next_string())) { projection.insert(*attr); }
	} else {
		return -2;
	}

	return projection.empty() ? 0 : 1;
}


/* 
 * It now prints chained attributes. Or it should.
 * 
 * It should add the server_time attribute if it's
 * defined.
 *
 * It should convert default IPs to SocketIPs.
 *
 * It should also do encryption now.
 */
int putClassAd ( Stream *sock, const classad::ClassAd& ad )
{
	int options = 0;
	return _putClassAd(sock, ad, options, nullptr);
}

int putClassAd (Stream *sock, const classad::ClassAd& ad, int options, const classad::References * whitelist /*=nullptr*/, const classad::References * encrypted_attrs /*=nullptr*/)
{
	int retval = 0;
	classad::References expanded_whitelist; // in case we need to expand the whitelist

	bool expand_whitelist = ! (options & PUT_CLASSAD_NO_EXPAND_WHITELIST);
	if (whitelist && expand_whitelist) {
		// Jaime made changes to the core classad lib that make this unneeded...
		//ad.InsertAttr("MY","SELF");
		for (classad::References::const_iterator attr = whitelist->begin(); attr != whitelist->end(); ++attr) {
			ExprTree * tree = ad.Lookup(*attr);
			if (tree) {
				expanded_whitelist.insert(*attr); // the node exists, so add it to the final whitelist
				if (dynamic_cast<classad::Literal *>(tree) == nullptr) {
					ad.GetInternalReferences(tree, expanded_whitelist, false);
				}
			}
		}
		//ad.Delete("MY");
		//classad::References::iterator my = expanded_whitelist.find("MY");
		//if (my != expanded_whitelist.end()) { expanded_whitelist.erase(my); }
		whitelist = &expanded_whitelist;
	}

	bool non_blocking = (options & PUT_CLASSAD_NON_BLOCKING) != 0;
	ReliSock* rsock = dynamic_cast<ReliSock*>(sock);
	if (non_blocking && rsock)
	{
		BlockingModeGuard guard(rsock, true);
		if (whitelist) {
			retval = _putClassAd(sock, ad, options, *whitelist, encrypted_attrs);
		} else {
			retval = _putClassAd(sock, ad, options, encrypted_attrs);
		}
		bool backlog = rsock->clear_backlog_flag();
		if (retval && backlog) { retval = 2; }
	}
	else // normal blocking mode put
	{
		if (whitelist) {
			retval = _putClassAd(sock, ad, options, *whitelist, encrypted_attrs);
		} else {
			retval = _putClassAd(sock, ad, options, encrypted_attrs);
		}
	}
	return retval;
}

// helper function for _putClassAd
static int _putClassAdTrailingInfo(Stream *sock, const classad::ClassAd& /* ad */, bool send_server_time, bool excludeTypes)
{
    if (send_server_time)
    {
        //insert in the current time from the server's (Schedd) point of
        //view. this is used so condor_q can compute some time values
        //based upon other attribute values without worrying about
        //the clocks being different on the condor_schedd machine
        // -vs- the condor_q machine

        static const char fmt[] = ATTR_SERVER_TIME " = %ld";
        char buf[sizeof(fmt) + 12]; //+12 for time value
        snprintf(buf, sizeof(buf), fmt, (long)time(NULL));
        if (!sock->put(buf)) {
            return false;
        }
    }

    //ok, so the name of the bool doesn't really work here. It works
    //  in the other places though.
    if (!excludeTypes)
    {
        // Now, we always send empty strings for the special-case
        // MyType/TargetType values at the end of the ad.
        if (!sock->put("") || !sock->put("")) {
            return false;
        }
    }

	return true;
}

int _putClassAd( Stream *sock, const classad::ClassAd& ad, int options,
	const classad::References *encrypted_attrs)
{
	bool excludeTypes = (options & PUT_CLASSAD_NO_TYPES) == PUT_CLASSAD_NO_TYPES;
	bool exclude_private = (options & PUT_CLASSAD_NO_PRIVATE) == PUT_CLASSAD_NO_PRIVATE;
	auto *verinfo = sock->get_peer_version();
	bool exclude_private_v2 = exclude_private || !verinfo || !verinfo->built_since_version(9, 9, 0);

	classad::ClassAdUnParser	unp;
	std::string					buf;
	buf.reserve(65536);
	bool send_server_time = false;

	unp.SetOldClassAd( true, true );

	int numExprs=0;

	classad::AttrList::const_iterator itor;
	classad::AttrList::const_iterator itor_end;

	bool haveChainedAd = false;

	const classad::ClassAd *chainedAd = ad.GetChainedParentAd();
	if(chainedAd){
		haveChainedAd = true;
	}

	bool crypto_is_noop = sock->prepare_crypto_for_secret_is_noop();

	// Remember whether the ad has any private attributes for the cases
	// where we care (when selectively encrypting them or when excluding
	// them).
	int private_count = 0;
	for(int pass = 0; pass < 2; pass++){

		/*
		* Count the number of chained attributes on the first
		*   pass (if any!), then the number of attrs in this classad on
		*   pass number 2.
		*/
		if(pass == 0){
			if(!haveChainedAd){
				continue;
			}
			itor = chainedAd->begin();
			itor_end = chainedAd->end();
		}
		else {
			itor = ad.begin();
			itor_end = ad.end();
		}

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;

			// This logic is paralleled below when sending the attributes.
			if (crypto_is_noop && !exclude_private && !exclude_private_v2) {
				// If the channel is encrypted or we can't encrypt, and
				// we're sending all attributes, then don't bother checking
				// if any attributes are private.
				numExprs++;
			} else {
				bool private_attr_v2 = ClassAdAttributeIsPrivateV2(attr);
				bool private_attr = private_attr_v2 || ClassAdAttributeIsPrivateV1(attr) ||
					(encrypted_attrs && (encrypted_attrs->find(attr) != encrypted_attrs->end()));
				if (private_attr) {
					private_count++;
				}
				if ((exclude_private && private_attr) || (exclude_private_v2 && private_attr_v2)) {
					// This is a private attribute that should be excluded.
					// Do not send it.
					continue;
				}
				numExprs++;
			}
		}
	}

	if( (options & PUT_CLASSAD_SERVER_TIME) != 0 ){
		//add one for the ATTR_SERVER_TIME expr
		numExprs++;
		send_server_time = true;
	}

	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}

	for(int pass = 0; pass < 2; pass++){
		if(pass == 0) {
			/* need to copy the chained attrs first, so if
				*  there are duplicates, the non-chained attrs
				*  will override them
				*/
			if(!haveChainedAd){
				continue;
			}
			itor = chainedAd->begin();
			itor_end = chainedAd->end();
		} 
		else {
			itor = ad.begin();
			itor_end = ad.end();
		}

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;
			classad::ExprTree const *expr = itor->second;

			bool encrypt_it = false;

			// This parallels the logic above that counts the number of
			// attributes to send, except that we skip checking for
			// private attributes if we know there aren't any.
			if ((crypto_is_noop && !exclude_private && !exclude_private_v2) || (private_count == 0)) {
				// We can send everything without special handling of
				// private attributes for one of these reasons:
				// 1) We're sending all attributes and either the channel
				//    is already encrypted or can't be encrypted.
				// 2) There are no private attributes to worry about.
			} else {
				bool private_attr_v2 = ClassAdAttributeIsPrivateV2(attr);
				bool private_attr = private_attr_v2 || ClassAdAttributeIsPrivateV1(attr) ||
					(encrypted_attrs && (encrypted_attrs->find(attr) != encrypted_attrs->end()));
				if ((exclude_private && private_attr) || (exclude_private_v2 && private_attr_v2)) {
					// This is a private attribute that should be excluded.
					// Do not send it.
					continue;
				}
				if (private_attr /* && !crypto_is_noop */) {
					// This is a private attribute and the channel is
					// currently unencrypted.
					// Turn on encryption for just this attribute.
					encrypt_it = true;
				}
			}

			buf = attr;
			buf += " = ";
			unp.Unparse( buf, expr );

			if (encrypt_it) {
				sock->put(SECRET_MARKER);

				sock->put_secret(buf.c_str());
			}
			else if (!sock->put(buf) ){
				return false;
			}
		}
	}

	return _putClassAdTrailingInfo(sock, ad, send_server_time, excludeTypes);
}

int _putClassAd( Stream *sock, const classad::ClassAd& ad, int options, const classad::References &whitelist, const classad::References *encrypted_attrs)
{
	bool excludeTypes = (options & PUT_CLASSAD_NO_TYPES) == PUT_CLASSAD_NO_TYPES;
	bool exclude_private = (options & PUT_CLASSAD_NO_PRIVATE) == PUT_CLASSAD_NO_PRIVATE;
	auto *verinfo = sock->get_peer_version();
	bool exclude_private_v2 = exclude_private || !verinfo || !verinfo->built_since_version(9, 9, 0);

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );

	classad::References blacklist;
	for (classad::References::const_iterator attr = whitelist.begin(); attr != whitelist.end(); ++attr) {
		if ( ! ad.Lookup(*attr) ||
			 (exclude_private &&
			  (ClassAdAttributeIsPrivateV1(*attr) ||
			   (encrypted_attrs && (encrypted_attrs->find(*attr) != encrypted_attrs->end())))
			  ) ||
			 (exclude_private_v2 && ClassAdAttributeIsPrivateV2(*attr))
			 ) {
			blacklist.insert(*attr);
		}
	}

	int numExprs = whitelist.size() - blacklist.size();

	bool send_server_time = false;
	if( (options & PUT_CLASSAD_SERVER_TIME) != 0 ){
		//add one for the ATTR_SERVER_TIME expr
		// if its in the whitelist but not the blacklist, add it to the blacklist instead
		// since its already been counted in that case.
		if (whitelist.find(ATTR_SERVER_TIME) != whitelist.end() && 
			blacklist.find(ATTR_SERVER_TIME) == blacklist.end()) {
			blacklist.insert(ATTR_SERVER_TIME);
		} else {
			++numExprs;
		}
		send_server_time = true;
	}


	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}

	std::string buf;
	buf.reserve(65536);
	bool crypto_is_noop =  sock->prepare_crypto_for_secret_is_noop();
	for (classad::References::const_iterator attr = whitelist.begin(); attr != whitelist.end(); ++attr) {

		if (blacklist.find(*attr) != blacklist.end())
			continue;

		classad::ExprTree const *expr = ad.Lookup(*attr);
		buf = *attr;
		buf += " = ";
		unp.Unparse( buf, expr );

		if ( ! crypto_is_noop &&
			(ClassAdAttributeIsPrivateAny(*attr) ||
			(encrypted_attrs && (encrypted_attrs->find(*attr) != encrypted_attrs->end())))
		) {
			if (!sock->put(SECRET_MARKER)) {
				return false;
			}
			if (!sock->put_secret(buf.c_str())) {
				return false;
			}
		}
		else if ( ! sock->put(buf)){
			return false;
		}
	}

	return _putClassAdTrailingInfo(sock, ad, send_server_time, excludeTypes);
}
