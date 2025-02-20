/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
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
#include "wire_ad_buffer.h"

bool WireClassadBuffer::putPairUnparsed(std::string_view attr, std::string_view rhs)
{
	if (cbAlloc == off) {
		grow(cbAlloc * 2);
	}

	// write the 'pair' tag and the attribute name, in compact or regular form
	if (attr.size() < 256) {
		putByte(classad::ExprStream::EsSmallPair);
		putByte((unsigned char)attr.size());
	} else {
		putByte(classad::ExprStream::EsBigPair);
		putInteger((unsigned int)attr.size());
	}
	putBytes(attr.data(), (unsigned int)attr.size());

	// put unparsed rhs as an envelope node
	if (rhs.size() < 256) {
		putByte(classad::ExprStream::EsEnvelope);
		putByte((unsigned char)rhs.size());
	} else {
		putByte(classad::ExprStream::EsBigEnvelope);
		putInteger((unsigned int)rhs.size());
	}
	putBytes(rhs.data(), (unsigned int)rhs.size());
	return true;
}

/*static*/ std::string_view WireClassadBuffer::unparsed_value(classad::ExprStream stm)
{
	unsigned char ct;
	if (stm.readByte(ct) && (ct == classad::ExprStream::EsEnvelope || ct == classad::ExprStream::EsBigEnvelope)) {
		std::string_view ret;
		if (stm.readSizeString(ret, ct == classad::ExprStream::EsEnvelope)) {
			// if there is a trailing null inside the view, put it outside.
			if (ret.back() == '\0') ret.remove_suffix(1);
			return ret;
		}
	}
	return std::string_view();
}

bool WireClassadBuffer::putPairFromWire(const char * line)
{
	std::string_view attr;
	const char * rhsptr;
	if ( ! SplitLongFormAttrValue(line, attr, rhsptr)) {
		return false;
	}

	// put the right hand side as a normal length prefixed string with a null terminator
	std::string_view rhs(rhsptr, strlen(rhsptr) + 1);
	return putPairUnparsed(attr, rhs);
}

bool WireClassadBuffer::putPair(std::string_view attr, const classad::ExprTree * expr)
{
	if (attr.length() < 256) {
		putByte(classad::ExprStream::EsSmallPair);
		putSmallString(attr);
	} else {
		// write the attribute name
		putByte(classad::ExprStream::EsBigPair);
		putString(attr);
	}
	putNullableExpr(expr);
	return true;
}


classad::ExprStreamMaker::Mark WireClassadBuffer::openAd(const std::string & label)
{
	putByte(classad::ExprStream::EsClassAd);
	putString(label);
	unsigned int size = 0;
	auto mk = mark(size); // reserve space for size field
	putByte(classad::ExprStream::EsBigHash);
	auto mkAttr = mark(size); // reserve space for the attr count
	updateAt(mkAttr, size);
	return mk;
}

// todo: add an optional build_index flag
int WireClassadBuffer::closeAd(const ExprStreamMaker::Mark & mkHash)
{
	// mkHash points to the size field before the 1 byte ExprStream::BigHash token
	// we also want a marker that points to the pair count after that
	unsigned int num_pairs = 0;

	// update the size field for the amount of data added after it, not including the size field
	unsigned int size = added(mkHash) - mkHash.len();
	updateAt(mkHash, size);

	// Now get a stream that encloses that range
	classad::ExprStream stm = stream_of(mkHash);

	unsigned char ct=0;
	stm.readByte(ct); // this should be BigHash

	// remember this position in case we need to update the pair count
	Mark mkNumPairs(mkHash.end()+sizeof(ct), sizeof(num_pairs));
	stm.readInteger(num_pairs); // num_pairs is likely to be 0 at this point.

	// num_pairs is not set, so count the pairs and update the count
	if ( ! num_pairs) {
		num_pairs = 0;

		classad::ExprTree::NodeKind kind;
		while (stm.readByte(ct)) {
			switch (ct) {
			case classad::ExprStream::EsBigPair:
			case classad::ExprStream::EsSmallPair:
				if ( ! stm.skipSizeStream(ct == classad::ExprStream::EsSmallPair)
					|| ! stm.skipNullableExpr(kind)) { goto bail; }
				++num_pairs;
				break;
			case classad::ExprStream::EsBigEqZPair:
			case classad::ExprStream::EsSmallEqZPair:
				if ( ! stm.skipSizeStream(ct == classad::ExprStream::EsSmallEqZPair)) { goto bail; }
				++num_pairs;
				break;
			case classad::ExprStream::EsBigComment:
			case classad::ExprStream::EsComment:
				if ( ! stm.skipSizeStream(ct == classad::ExprStream::EsComment)) { goto bail; }
			default: // unexpected
				goto bail;
			}
		}
		updateAt(mkNumPairs, num_pairs);
	}
	return num_pairs;
bail:
	return 0;
}

bool WireClassadBuffer::build_index()
{
	classad::ExprStream stmAd = stream();

	std::string_view label;
	auto stmHash = find_hash(stmAd, label);
	if (stmHash.empty()) {
		return false;
	}

	int numExprs = read_hash(stmHash);
	if (numExprs <= 0) {
		return false;
	}

	// make sure we have room for the index. it will consist of an int offset and 2 int sizes for each pair
	int cbindex = sizeof(index_entry) * numExprs;
	if (off+cbindex > cbAlloc) {
		grow(off + cbindex);
		// our base pointer has probably moved, so we must re-find the hash
		stmAd = stream();
		stmHash = find_hash(stmAd, label);
		int num = read_hash(stmHash);
		ASSERT(num == numExprs);
	}

	ioff = off; // put index after the classad data
	inum = 0;
	index_entry *pi = (index_entry *)(base + ioff);

	for (int ix = 0; ix < numExprs; ++ix) {
		std::string_view attr;
		binary_span expr;
		classad::ExprTree::NodeKind kind;
		if ( ! next_pair(stmHash, attr, expr, kind) || attr.empty() || expr.empty()) {
			continue;
		}
		pi[inum].nameoff = (unsigned int)(attr.data() - (const char *)base);
		pi[inum].namelen = (unsigned int)attr.size();
		pi[inum].exprlen = (unsigned int)expr.size();
		++inum;
	}
	std::sort(pi, pi+inum, IndexOrder(base));
	return inum == (unsigned int)numExprs;
}


// if the WireClassAdBuffer contains a classad or a bare hashtable, find the hashtable
// and set the given ExprStream to it's start and length
// 
// The caller can then call one of the next_pair() methods up to attr_count times
// passing the same ExprStream each time to walk the pairs in the hashtable.
//
/*static*/ classad::ExprStream WireClassadBuffer::find_hash(classad::ExprStream & stm, std::string_view & label)
{
	classad::ExprStream hashStm;
	unsigned char ct = 0;
	if (stm.peekByte(ct) && ct == classad::ExprStream::EsClassAd) {
		stm.readByte(ct);
		stm.readString(label);
		stm.readStream(hashStm);
	} else if (ct == classad::ExprStream::EsHash || ct == classad::ExprStream::EsBigHash) {
		label = std::string_view(); // empty label
		hashStm = stm;
	}
	return hashStm;
}

/*static*/ int WireClassadBuffer::read_hash(classad::ExprStream & stm)
{
	unsigned int attr_count = 0;
	unsigned char ct;
	if ( ! stm.readByte(ct)) {
		return 0;
	}
	if (ct == classad::ExprStream::EsHash) {
		unsigned char count = 0;
		stm.readByte(count);
		attr_count = count;
	} else if (ct == classad::ExprStream::EsBigHash) {
		stm.readInteger(attr_count);
	}
	return attr_count;
}


// get the next attr/expr pair from within the ExprStream. 
// use this method when the WireClassadBuffer is binary
// It is expected that the ExprStream was initialized by calling find_hash above.
// in this case the right hand side will be a Pickled ExprTree
/*static*/ bool WireClassadBuffer::next_pair(
	classad::ExprStream & stm,
	std::string_view & attr,
	binary_span & rhs,
	classad::ExprTree::NodeKind & kind)
{
	unsigned char ct;
	if ( ! stm.peekByte(ct))
		return false;

	// skip comments
	while (ct == classad::ExprStream::EsComment || ct == classad::ExprStream::EsBigComment) {
		stm.readByte(ct);
		if ( ! stm.skipSizeStream(ct == classad::ExprStream::EsComment)) {
			return false;
		}
		if ( ! stm.peekByte(ct))
			return false;
	}

	if (ct == classad::ExprStream::EsSmallPair || ct == classad::ExprStream::EsBigPair) {
		stm.readByte(ct);
		if ( ! stm.readSizeString(attr, ct == classad::ExprStream::EsSmallPair)) return false;
	} else {
		return false;
	}

	rhs = stm.readExprBytes(kind);
	return ! attr.empty();
}

bool getClassAdRaw(Stream *sock, WireClassadBuffer & wab)
{
	int numExprs;
	const char * strptr;

	sock->decode( );

	int64_t time_and_flags = 0;
	if( !sock->code(time_and_flags)) {
		return false;
	}
	wab.clear(time_and_flags); // sets is_binary() and (if binary) also server_time() of the wab.

	bool is_binary = wab.is_binary();
	if (is_binary) {
		int numBytes = 0;
		if (!sock->code(numBytes)) {
			return false;
		}

		// read from the socket into the ExprStreamMaker's buffer
		unsigned char * buf = wab.reserve(numBytes);
		if (!buf)
			return false;
		int bytes_remaining = numBytes;
		while (bytes_remaining > 0) {
			int got_bytes = sock->get_bytes(buf, bytes_remaining);
			if (got_bytes <= 0) {
				return false;
			}
			bytes_remaining -= got_bytes;
			buf += got_bytes;
		}
		wab.advance(numBytes);
		return true;

	} else if (time_and_flags < 0) {
		// a negative value that does not set BINARY_CLASSAD_FLAG is invalid
		return false;
	}

	// this is a long form classad, a sequence of strings.
	numExprs = (int)time_and_flags;

	wab.reserve((numExprs+6) * 128);  // guess about how much space we will need

	// we will encode what we get off the wire in a form similar to the binary form
	// but without the enclosing ExprStream::ClassAd tag and label.
	// we do this so that we can use ClassAd::Update() methods later without
	// having to keep track of binary vs. long form.  see updateAdFromRaw below

	// put the 'hashtable' tag, and the number of attributes
	wab.putByte(classad::ExprStream::EsBigHash);
	wab.putInteger((unsigned int)numExprs);

	// put each line as a ExprStream::Pair
	for( int ii = 0 ; ii < numExprs ; ++ii ) {
		strptr = NULL;
		int cb = 0;
		if ( ! sock->get_string_ptr(strptr, cb) || ! strptr) {
			return( false );
		}

		// if string is the magic SECRET_MARKER string, then the *next* string is encrypted, and the actual key=value pair.
		if ((*strptr=='Z') && strptr[1] == 'K' && strptr[2] == 'M' && strptr[3] == 0) {
			if ( ! sock->get_secret(strptr, cb) || ! strptr) {
				dprintf(D_FULLDEBUG, "getClassAdRaw Failed to read encrypted ClassAd expression.\n");
				break;
			}
		}

		wab.putPairFromWire(strptr);
	}

	// We fetch but ignore the special MyType and TargetType fields.
	// These attributes, if defined, are included in the regular set
	// of name/value pairs.
	if (!sock->get_string_ptr(strptr)) {
		dprintf(D_FULLDEBUG, "FAILED to get(inputLine)\n" );
		return false;
	}

	if (!sock->get_string_ptr(strptr)) {
		dprintf(D_FULLDEBUG, "FAILED to get(inputLine) 2\n" );
		return false;
	}

	return true;
}

// TODO: can we assume an empty input ad? or do we also need to delete attributes?
// TODO: pass options to Update and UpdateViaCache

bool updateAdFromRaw(classad::ClassAd & ad, int64_t time_and_flags, classad::ExprStream & stmAd, int options, const classad::References * whitelist)
{
	bool use_cache = (options & GET_CLASSAD_NO_CACHE) == 0;
	//bool cache_lazy = (options & GET_CLASSAD_LAZY_PARSE) != 0;
	bool fast_tricks = (options & GET_CLASSAD_FAST) != 0;
	const size_t always_cache_string_size = 128; // no fast parse for strings > this size.

	if ( ! (options & GET_CLASSAD_NO_CLEAR)) {
		ad.Clear();
	}

	// the ad may start with the 0xCA tag that indicates a complete
	// top level add.
	// ad.Update wants the hashtable, so we need to scan down to it.
	std::string_view label;
	auto stmHash = WireClassadBuffer::find_hash(stmAd, label);
	if (stmHash.empty()) {
		return false;
	}

	if (time_and_flags & BINARY_CLASSAD_FLAG) {

		// set a ServerTime attribute from the server time stored in the binary header
		// This will be overwritten by ad.Update below if the binary blob has a ServerTime attribute.
		// TODO: add an options flag for this??
		time_t server_time = (time_t)(time_and_flags & ~BINARY_CLASSAD_MASK);
		if (server_time && ( ! whitelist || whitelist->count(ATTR_SERVER_TIME))) {  
			ad.Assign(ATTR_SERVER_TIME, server_time);
		}

		// peek at first pair to see if fast_tricks is possible
		classad::ExprStream stmTmp = stmHash;
		int numExprs = WireClassadBuffer::read_hash(stmTmp);
		if (numExprs <= 0) return false;

		std::string_view lhs;
		binary_span expr;
		classad::ExprTree::NodeKind kind;
		if (WireClassadBuffer::next_pair(stmTmp, lhs, expr, kind) && kind != classad::ExprTree::EXPR_ENVELOPE) {
			// fast tricks only work with envelope expr trees
			fast_tricks = false;
		}
	}

	// if we have not been asked to use fast parsing tricks
	// we can just call the classad update from pickle functions
	if ( ! fast_tricks) {
		if (whitelist) {
			return ad.Update(stmHash, *whitelist, false);
		}
		return ad.UpdateViaCache(stmHash);
	}

	// can't use ad.update because of fast tricks
	// so iterate the hash here

	int numExprs = WireClassadBuffer::read_hash(stmHash);
	if (numExprs <= 0) {
		return false;
	}

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);

	std::string attr; // TODO: remove this when we can insert into a classad froma string_view

	std::string_view lhs;
	binary_span expr;
	classad::ExprTree::NodeKind kind;

	for (int ix = 0; ix < numExprs; ++ix) {
		if ( ! WireClassadBuffer::next_pair(stmHash, lhs, expr, kind) || lhs.empty() || expr.empty()) {
			dprintf(D_ALWAYS, "updateAdFromRaw next_pair did not return anything to insert\n");
			return false;
		}

		// TODO: remove this when we can insert into classads using a string_view
		attr.assign(lhs);

		// honor whitelist if any
		if (whitelist && ! whitelist->count(attr)) {
			continue;
		}

		classad::ExprStream stmExpr(expr);
		if (kind != classad::ExprTree::EXPR_ENVELOPE) {
			classad::ExprTree * tree = nullptr;
			if (stmExpr.readNullableExpr(tree)) {
				ad.Insert(attr, tree);
			} else {
				dprintf(D_ALWAYS, "updateAdFromRaw next_pair rhs is not a pickled expr\n");
			}
			continue;
		}

		// rhs is an Envelope, which is basically an unparsed expression string
		unsigned char ct2;
		std::string_view rhs;
		stmExpr.readByte(ct2);
		stmExpr.readSizeString(rhs, ct2 == classad::ExprStream::EsEnvelope);

		// Fast tricks pre-parses the right hand side when it is detected as a simple literal
		// this is faster than letting the classad parser parse it (as of 8.7.0) and also
		// uses less memory than letting the classad cache see it since literal nodes are the same
		// size as envelope nodes.
		//
		bool inserted = false;
		if (fast_tricks) {
			if (rhs.back() == '\0') rhs.remove_suffix(1);  // size should not include the trailing \0
			classad::Literal* lit = fastParseSomeClassadLiterals(rhs.data(), rhs.size(), always_cache_string_size);
			if (lit) {
				inserted = ad.InsertLiteral(attr, lit);
			}
		}

		if (inserted) {
		} else {
			// we can't cache nested classads or lists, so just parse and insert them
			bool cache = use_cache && (rhs.front() != '[' && rhs.front() != '{');
			if (cache) {
				inserted = ad.InsertViaCache(attr, rhs.data());
			} else {
				ExprTree *tree = parser.ParseExpression(rhs.data());
				if (tree) {
					inserted = ad.Insert(attr, tree);
				}
			}
		}
		if ( ! inserted) {
			dprintf(D_ALWAYS, "updateAdFromRaw FAILED to insert %s\n", rhs.data());
			return false;
		}
	}

	return true;
}




