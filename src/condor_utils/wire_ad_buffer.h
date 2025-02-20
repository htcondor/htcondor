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

#ifndef _wire_ad_buffer_H
#define _wire_ad_buffer_H


// This class is designed to allow us to read classads off the wire into an ExprStreamMaker buffer
// This allows separation of taking the classad off the wire from parsing it.
// The wire buffer can hold pickled ads or standard wire form (-long) ads
// The ads are stored as a sequence of ExprStream::PairBase pairs
// For ads in wire/long form the pair will be of type (ExprStream::AttrBase,ExprStream::Unparsed)
// For pickled ads the pair will be (ExprStream::AttrBase,<any-pickled-exprtree>)
//
class WireClassadBuffer : public classad::ExprStreamMaker {
public:
	WireClassadBuffer() : time_and_flags(0), ioff(0), inum(0) {}
	~WireClassadBuffer() { time_and_flags = 0; };

	void clear(int64_t t_and_f) {
		time_and_flags = t_and_f;
		off = 0; ioff = inum = 0;
	}
	int64_t get_time_and_flags() const { return time_and_flags; }
	bool is_binary() const { return (time_and_flags & BINARY_CLASSAD_FLAG) != 0; }
	time_t get_servertime() const { return (time_t)(time_and_flags & ~BINARY_CLASSAD_MASK); }
	bool empty() const { return off == 0; }

	// returns an ExprStream that still refers to the buffer owned by this class
	classad::ExprStream stream() const { classad::ExprStream stm(base, off); return stm; }

	// swaps the buffer in this class with the supplied buffer and clears the index
	unsigned char * swap_buffer(unsigned char * buf, unsigned int & size, unsigned int & capacity, int64_t & t_and_f) { 
		int64_t t = time_and_flags;
		time_and_flags = t_and_f;
		t_and_f = t;

		unsigned int cb = off;
		off = size;
		size = cb;

		cb = cbAlloc;
		cbAlloc = size;
		capacity = cb;

		ioff = inum = 0; // clear the index

		unsigned char * ret = base;
		base = buf;
		return ret;
	}

	// the two methods below allow us to get a buffer and fetch data from a socket directly into the buffer

	// reserve space, and give back a pointer to the buffer
	unsigned char * reserve(unsigned int size) {
		if ( ! base || (size > cbAlloc)) { grow(size); }
		return base;
	}
	// advance the 'data used' offset to account for data written directly to the buffer
	int advance(unsigned int size) { off += size; return size; }

	// put SmallPair or BigPair from one line of a wire form classad from the wire.
	// This line is expected to be in the form attr=exprssion and to be null terminated,
	// i.e. what we get back from sock->get_string_ptr(). the rhs of the pair will be stored with a \0 terminator
	bool putPairFromWire(const char * line);
	// put SmallPair or BigPair , and an Unparsed rhs.  the rhs is expected to contain a \0 terminator
	bool putPairUnparsed(std::string_view attr, std::string_view rhs);

	bool putPair(std::string_view attr, const classad::ExprTree * expr);

	// start a ClassAd, returns a mark to the start of the BigHash
	ExprStreamMaker::Mark openAd(const std::string & label);
	// pass back the mark returned from openAd
	int closeAd(const ExprStreamMaker::Mark & mkHash);

	// call after closeAd to build an index for the ad so you can call Lookup
	// returns false if index could not be built or it is incomplete
	bool build_index();
	// if index has been built (including a partial build) returns the number of index entries
	unsigned int index_size() const { return inum; }
	// lookup an attribute in the index and return an ExprStream for the right hand side
	bool Lookup(std::string_view attr, classad::ExprStream * stm = nullptr, classad::ExprTree::NodeKind * kind = nullptr) {
		if ( ! inum)
			return false;

		IndexOrder order(base,attr);
		index_entry * pi = (index_entry*)(base + ioff);
		index_entry * pend = pi + inum;
		auto lb = std::lower_bound(pi, pend, attr, order);
		if (lb != pend && order.equal(*lb, attr)) {
			if (stm) {
				*stm = classad::ExprStream(base + lb->nameoff + lb->namelen, lb->exprlen);
				if (kind) { stm->peekKind(*kind); }
			}
			return true;
		}
		return false;
	}

	// returns a vector of *unique* keys from the index (i.e. duplicates are eliminated)
	// Because the index is sorted so that duplicate keys that are later in the
	// hash are earlier in the index, we can eliminate dups by skipping an index entry
	// when it would have the same key value as previous returned key.
	std::vector<std::string_view> keys() {
		std::vector<std::string_view> kl;
		kl.reserve(inum);
		index_entry * pi = (index_entry*)(base + ioff);
		index_entry * pend = pi + inum;
		while (pi < pend) {
			if (kl.empty() ||
				pi->namelen != kl.back().size() || 
				strncasecmp((char*)base + pi->nameoff, kl.back().data(), pi->namelen))
			{
				kl.emplace_back((char*)base + pi->nameoff, pi->namelen);
			}
			++pi;
		}
		return kl;
	}

	// turn a ExprStream that as returned by lookup back into an unparsed string
	// use only when is_binary() is false.
	static std::string_view unparsed_value(classad::ExprStream stm);

	// sets stm to the stream containing the Hash or BigHash tag
	static classad::ExprStream find_hash(classad::ExprStream & stmAd, std::string_view & label);
	// reads attribute count from a stream that starts with Hash or BigHash
	static int  read_hash(classad::ExprStream & stmHash);

	// gets the next pair from a Hash or BigHash stream that has pickled or unparsed expressions, kind indicates which
	static bool next_pair(classad::ExprStream & stmHash, std::string_view & attr, binary_span & rhs, classad::ExprTree::NodeKind & kind);

private:
	int64_t time_and_flags;
	unsigned int ioff; // offset of index of pairs, sorted by attribute
	unsigned int inum; // number of entries in index of pairs
	struct index_entry { unsigned int nameoff; unsigned int namelen; unsigned int exprlen; };

	// helper for building and using the index 
#if 0
	struct index_less {
		const char *base;
		index_less(const unsigned char *_b) : base((const char *)_b) {}
		bool operator()(index_entry &a, index_entry &b) noexcept {
			if (a.namelen < b.namelen) return true;
			if (a.namelen > b.namelen) return false;
			return strncasecmp(base + a.nameoff, base + b.nameoff, a.namelen) < 0;
		}
	};
#else
	struct IndexOrder {
		const char * ibase{nullptr};
		const size_t len{0}; // captures the length of the string we are comparing to
		IndexOrder(const unsigned char *ib, std::string_view s) : ibase((const char*)ib), len(s.size()) {}
		IndexOrder(const unsigned char *ib) : ibase((const char*)ib) {}

		// this one is used by std::sort
		bool operator()(const index_entry &a, const index_entry &b) noexcept {
			if (a.namelen < b.namelen) return true;
			if (a.namelen > b.namelen) return false;
			int rv = strncasecmp(ibase + a.nameoff, ibase + b.nameoff, a.namelen);
			// the index can have duplicate keys for job classads (cluster+proc ad)
			// we want the later key to sort first so that lookup finds it
			if (rv == 0) { rv = (int)b.nameoff - (int)a.nameoff; }
			return rv < 0;
		}

		// this one is used by Lookup
		bool operator()(const index_entry &lhs, std::string_view rhs) noexcept {
			if (lhs.namelen < this->len) return true;
			if (lhs.namelen > this->len) return false;
			return strncasecmp(ibase + lhs.nameoff, rhs.data(), len) < 0;
		}
		bool equal(const index_entry &lhs, std::string_view rhs) noexcept {
			if (lhs.namelen != this->len) return false;
			return strncasecmp(ibase + lhs.nameoff, rhs.data(), len) == 0;
		}
	};
#endif

};

bool getClassAdRaw(Stream* sock, WireClassadBuffer & wab);
// use GET_CLASSAD_* options flags for options
bool updateAdFromRaw(classad::ClassAd & ad, int64_t time_and_flags, classad::ExprStream & stmAd, int options, const classad::References * whitelist);
inline bool updateAdFromRaw(classad::ClassAd & ad, const WireClassadBuffer & wab, int options, const classad::References * whitelist) {
	classad::ExprStream stmAd = wab.stream();
	return updateAdFromRaw(ad, wab.get_time_and_flags(), stmAd, options, whitelist);
}


#endif
