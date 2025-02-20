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


#ifndef __CLASSAD_CLASSAD_H__
#define __CLASSAD_CLASSAD_H__


#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include "classad/classad_containers.h"
#include "classad/exprTree.h"
#include "classad/classad_flat_map.h"
#include "classad/flat_set.h"

#ifdef TJ_PICKLE
#include <span>
using binary_span  = std::span<const unsigned char>;
#endif

namespace classad {

typedef flat_set<std::string, CaseIgnLTStr> References;
typedef flat_set<std::string, CaseIgnSizeLTStr> ReferencesBySize;
typedef std::map<const ClassAd*, References> PortReferences;

using AttrList = ClassAdFlatMap;

typedef std::set<std::string, CaseIgnLTStr> DirtyAttrList;

void ClassAdLibraryVersion(int &major, int &minor, int &patch);
void ClassAdLibraryVersion(std::string &version_string);

// Should parsed expressions be cached and shared between multiple ads.
// The default is false.
void ClassAdSetExpressionCaching(bool do_caching);
void ClassAdSetExpressionCaching(bool do_caching, int min_string_size);
bool ClassAdGetExpressionCaching();
bool ClassAdGetExpressionCaching(int & min_string_size);
extern size_t  _expressionCacheMinStringSize;

// This flag is only meant for use in Condor, which is transitioning
// from an older version of ClassAds with slightly different evaluation
// semantics. It will be removed without warning in a future release.
// The function SetOldClassAdSemantics() should be used instead of
// directly setting _useOldClassAdSemantics.
extern bool _useOldClassAdSemantics;
void SetOldClassAdSemantics(bool enable);

#ifdef TJ_PICKLE

// a class used to read a pickled classad from memory. essentially a read-only structured byte stream
//
// An instance of this class is passed a pointer and byte length into a buffer
// and this class then used to read structured data from the buffer.  it will
// not read past the length given to it in the constructor, or will it free the pointer it was passed
//
class ExprStream
{
protected:
	// pointer is NOT owned by this class, DO NOT FREE FROM HERE
	const unsigned char * base{nullptr}; // pointer to an external buffer containing pickled classad.
	unsigned int    cb{0};         // number of bytes this class is allowed to read from the pointer above
	unsigned int    off{0};        // current offset into the buffer, advanced as the stream is read
public:
	ExprStream() = default;
	ExprStream(const unsigned char * ptr, unsigned int size) : base(ptr), cb(size), off(0) {}
	ExprStream(const char * ptr, unsigned int size) : base((const unsigned char*)ptr), cb(size), off(0) {}
	ExprStream(const std::u8string & bindata) : base((const unsigned char*)bindata.data()), cb(bindata.size()), off(0) {}
	ExprStream(binary_span bindata) : base(bindata.data()), cb(bindata.size()), off(0) {}
	ExprStream(const ExprStream & that) = default; //: base(that.base), cb(that.cb), off(that.off) {}

	const unsigned char * data() const { return base; }
	unsigned int size() const { return cb; }

	// pickle token codes
	// pickle code depends on the fact that
	//    Literals   are 0x80 to 0x9F
	//    Operators  are 0xA0 to 0xBF (and are currently in the same order as the operators)
	//    Attributes are 0xE0 to 0xEF (of which pairs are 0xE8 to 0xEF)
	//    Misc Exprs are 0xF0 to 0xFF (fncall, list, nested ads, and unparsed exprs)
	enum {
		EsNoKind   = 0x7E,      // return from tokToGeneralKind and tokToLiteralKind when there is no mapping for the token
		EsBadTag   = 0x7F,      // return from tokToGeneralKind and tokToLiteralKind when tok is invalid

		EsLitBase  = 0x80, // Literal/Value types
		EsLitError = EsLitBase, // 0x80  ERROR_LITERAL and ERROR_VALUE no payload
		EsLitUndef,             // 0x81  UNDEFINED_LITERAL and UNDEFINED_VALUE no payload
		EsBoolFalse,            // 0x82  BOOLEAN_LITERAL with value false
		EsBoolTrue,             // 0x83  BOOLEAN_LITERAL with value true
		EsIntZero,              // 0x84  INTEGER_LITERAL with value of 0
		EsIntOne,               // 0x85  INTEGER_LITERAL with value of 1
		EsFloatZero,            // 0x86  REAL_LITERAL with a value of 0
		EsEmptyString,          // 0x87..STRING_LITERAL with a value of ""

		EsInt8, EsInt16, EsInt32, EsInt64, // 0x88 to 0x8B INTEGER_LITERAL of 1,2,4, or 8 bytes
		EsFloat8, EsFloat16, EsFloat32,    // 0x8C to 0x8E REAL_LITERAL, stored as 1,2,4 or 8 byte int
		EsDouble,                // 0x8F  REAL_LITERAL stored as 8 byte double

		// 0x90
		EsLitRelTime,            // 0x90  RELTIME_LITERAL and RELATIVE_TIME_VALUE an 8 byte double time in seconds
		EsLitAbsTime,            // 0x91  ABSTIME_LITERAL and ABSOLUTE_TIME_VALUE a 12 byte abstime_t, time_t + int
		EsString,                // 0x92  STRING_LITERAL uchar8 count + string, null term not guaranteed
		EsBigString,             // 0x93  STRING_LITERAL uint32 count + string, null term not guaranteed
		EsSimpleStr,             // 0x94  future unescaped STRING_LITERAL uchar8 count + string, null term not guaranteed
		EsBigSimpleStr,          // 0x95  future unescaped STRING_LITERAL uint32 count + string, null term not guaranteed
		// 0x96 thru 0x98 for future use
		EsListVal  = 0x99,       // 0x99  for ValueType::LIST_VALUE and SLIST_VALUE only, not for lists within a classad, see List and BigList below
		EsClassadVal,            // 0x9A  for ValueType::CLASSAD_VALUE and SCLASSAD_VALUE only, not for top level ClassAds or nested ads, see Classad and Hash below
		EsNullVal,               // 0x9F  for ValueType::NULL_VALUE

		EsOpBase   = 0xA0,       // Operation, there are 28 op codes these should be in the same order as OpKind enum
		EsElvisOp = EsOpBase | 0x1C,   // reserve space for future ?: operator
		EsTernaryOp = EsOpBase | 0x1D, // last op is TERNARY_OP=29 which is 0x1D
		EsOpEnd    = 0xBF,

		EsComment  = 0xC8,   // a small, length prefixed string that exists within the ExprStream, but is skipped when parsing ads
		EsBigComment = 0xC9, // also a string, but with a 32 bit length
		EsClassAd  = 0xCA,   // a top level classad (a nested ad would one of Hash or BigHash below)
		// top level ads have overall size field and a label field useful for scanning quickly

		// attributes extend from AttrBase to AttrBase + 0x07
		EsAttrBase = 0xE0, // AttributeReference, bits 0 & 1 encode absolute flag and presence of expression
		EsSmallAttr = EsAttrBase,		// attr of < 255 bytes
		EsAbsAttr  = EsAttrBase | 0x01, // AttributeReference absolute=true, expr=null.  i.e. .foo     (this is very uncommon)
		EsExprAttr = EsAttrBase | 0x02, // AttributeReference absolute=false, expr=<expression>.  i.e.  [ A=10; B=12; ].A
		EsBigAttr  = EsAttrBase | 0x04, // attr > 255 bytes in size

		EsPairBase = 0xE8, // from PairBaseX+0 to PairBase+7 are forms of Attr=Value pairs
		EsSmallPair = EsPairBase, // attr=expr pair where expr is a pickled expr tree and attr is < 256 bytes
		EsBigPair,                // attr=expr pair where expr is a pickled expr tree and attr is >= 256 bytes
		EsSmallEqZPair,           // attr = unparsed-expr\0 - a null terminated string, the traditional wire form
		EsBigEqZPair,             // attr = unparsed-expr\0, where the whole line is >= 256 bytes

		EsTreeBase = 0xF0,           // misc expr tree types
		EsCall     = EsTreeBase | 0, // FN_CALL_NODE function with < 256 arguments and function name < 256 bytes, use BigCall for bigger
		EsList     = EsTreeBase | 1, // EXPR_LIST_NODE ExprList with < 256 items, use BigList for larger
		EsHash     = EsTreeBase | 2, // CLASSAD_NODE ClassAd with < 256 attributes, use BigHash for larger
		EsEnvelope = EsTreeBase | 3, // EXPR_ENVELOPE unparsed expression < 256 bytes used when capturing off the wire, or for -long form ads
		//LitTree  = TreeBase | 6, // 6 all literal nodes use LitBase or ByteValBase above
		EsBigCall  = EsCall | 0x04,     // FN_CALL_NODE with function name > 255 characters or > 255 arguments
		EsBigList  = EsList | 0x04,     // EXPR_LIST_NODE ExprList with > 255 items
		EsBigHash  = EsHash | 0x04,     // CLASSAD_NODE ClassAd with > 255 items
		EsBigEnvelope = EsEnvelope | 0x04, // EXPR_ENVELOPE unparsed wire form expression > 255 bytes used when capturing off the wire, or for -long form ads
		// 0xF8 to 0xFE is unused
		EsNullTree = 0xFF,            // use to store a NULL rather than an expression (this is not the same as NULL_VALUE)

	};

	// map an ExprStream token code to a NodeKind
	// Note that this maps Pairs to ATTRREF, Comment and ClassAd into OP and ValueType into LITERAL
	// tokToGeneralKind returns ExprTree::NodeKind::ERROR_LITERAL for tokens in the literal or value range including future tokens
	// But it's used only by the ExprTree class which doesn't care
	static unsigned char tokToGeneralKind(unsigned char ct) {
		if (ct >= ExprStream::EsTreeBase) {
			if (ct > EsBigEnvelope) return EsNullTree;
			constexpr unsigned char ctok[]{
				ExprTree::NodeKind::FN_CALL_NODE,
				ExprTree::NodeKind::EXPR_LIST_NODE,
				ExprTree::NodeKind::CLASSAD_NODE,
				ExprTree::NodeKind::EXPR_ENVELOPE,
				};
			return ctok[ct&3];
		} else if (ct >= ExprStream::EsAttrBase) {
			// note this range includes the Pair tokens
			return ExprTree::NodeKind::ATTRREF_NODE;
		} else if (ct >= ExprStream::EsOpBase) {
			// note this range includes the Comment and ClassAd tokens
			return ExprTree::NodeKind::OP_NODE;
		} else if (ct >= ExprStream::EsLitBase) {
			// note this range includes the ValueTypes that are not literals
			return ExprTree::NodeKind::ERROR_LITERAL; // use one of  *_LITERAL kinds which calls Literal::Make
		}
		return EsBadTag; // ct is not within the range of pickle tokens
	}
	// toktoLiteralKind decodes the literal range, but not the other token ranges
	static unsigned char tokToLiteralKind(unsigned char ct) {
		if (ct < ExprStream::EsLitBase) {
			return EsBadTag; // ct is not within the range of pickle tokens
		} else if (ct < EsInt8) {
			// forms with no payload
			constexpr unsigned char ctok[]{
				ExprTree::NodeKind::ERROR_LITERAL,     //EsLitError    0x80  ERROR_LITERAL and ERROR_VALUE no payload
				ExprTree::NodeKind::UNDEFINED_LITERAL, //EsLitUndef    0x81  UNDEFINED_LITERAL and UNDEFINED_VALUE no payload
				ExprTree::NodeKind::BOOLEAN_LITERAL,   //EsBoolFalse   0x82  BOOLEAN_LITERAL with value false
				ExprTree::NodeKind::BOOLEAN_LITERAL,   //EsBoolTrue    0x83  BOOLEAN_LITERAL with value true
				ExprTree::NodeKind::INTEGER_LITERAL,   //EsIntZero     0x84  INTEGER_LITERAL with value of 0
				ExprTree::NodeKind::INTEGER_LITERAL,   //EsIntOne      0x85  INTEGER_LITERAL with value of 1
				ExprTree::NodeKind::REAL_LITERAL,	   //EsFloatZero   0x86  REAL_LITERAL with a value of 0
				ExprTree::NodeKind::STRING_LITERAL,	   //EsEmptyString 0x87  STRING_LITERAL with a value of ""
			};
			return ctok[ct&7];
		} else if (ct <= EsInt64) {
			return ExprTree::NodeKind::INTEGER_LITERAL;
		} else if (ct <= EsDouble) {
			return ExprTree::NodeKind::REAL_LITERAL;
		} else if (ct <= EsBigSimpleStr) {
			constexpr unsigned char ctok[]{
				ExprTree::NodeKind::RELTIME_LITERAL, // EsLitRelTime  0x90  RELTIME_LITERAL and RELATIVE_TIME_VALUE an 8 byte double time in seconds
				ExprTree::NodeKind::ABSTIME_LITERAL, // EsLitAbsTime  0x91  ABSTIME_LITERAL and ABSOLUTE_TIME_VALUE a 12 byte abstime_t, time_t + int
				ExprTree::NodeKind::STRING_LITERAL,  // EsString      0x92  STRING_LITERAL uchar8 count + string, null term not guaranteed
				ExprTree::NodeKind::STRING_LITERAL,  // EsBigString   0x93  STRING_LITERAL uint32 count + string, null term not guaranteed
				ExprTree::NodeKind::STRING_LITERAL,  // EsSimpleStr   0x94  future unescaped STRING_LITERAL uchar8 count + string, null term not guaranteed
				ExprTree::NodeKind::STRING_LITERAL,  // EsBigSimpleStr 0x95  future unescaped STRING_LITERAL uint32 count + string, null term not guaranteed
				EsNoKind, // future
				EsNoKind, // future
			};
			return ctok[ct&0x07];
		}
		return EsNoKind; // ct is not within the range of literal pickle tokens
	}

	class Mark {
		unsigned int m{0};
		Mark() = default;
		Mark(unsigned int o) : m(o) {}
		Mark &operator=(const Mark & that) = default; //{ m = that.m; return *this; }
		friend class ExprStream;
	};

	// return a position in the buffer.  this mark can later be used to rewind
	// back to that buffer posistion. used mostly when parsing fails.
	Mark mark() const {
		Mark mk(off);
		return mk;
	}
	void unwind(const Mark & mk) {
		if (mk.m <= cb) off = mk.m;
	}
	unsigned int size(const Mark & mk) {
		return off - mk.m;
	}
	const unsigned char * position(const Mark & mk) {
		return base + mk.m;
	}

	bool empty() const { return cb == 0 || off >= cb; }
	bool hasBytes(unsigned int cch) const {
		if ( ! base) return false;
		return (off+cch) <= cb;
	}

	bool peekByte(unsigned char & b) const {
		if (off >= cb)
			return false;
		b = base[off];
		return true;
	}

	bool readByte(unsigned char & b) {
		if (off >= cb)
			return false;
		b = base[off];
		++off;
		return true;
	}

	const char * readChars(unsigned int cch, std::string_view & attr) {
		if ( ! hasBytes(cch)) return NULL;
		const char * p = (const char *)(base+off);
		attr = std::string_view(p, cch);
		off += cch;
		return attr.data();
	}

	const char* readChars(unsigned int cch, std::string * attr = nullptr) {
		std::string_view sv;
		if ( ! readChars(cch, sv)) return nullptr;
		if (attr) { attr->assign(sv); return attr->c_str(); }
		return sv.data();
	}

	const unsigned char *readBytes(unsigned int size, ExprStream * stm = nullptr) {
		if ( ! hasBytes(size)) return NULL;
		const unsigned char * p = base+off;
		if (stm) { stm->base = p; stm->off = 0; stm->cb = size; }
		off += size;
		return p;
	}

	// read a POD type, bytes, ints and doubles
	template <typename T>
	bool readInteger(T & val) {
		if ( ! hasBytes(sizeof(val)))
			return false;
		// should this be network byte order here?
		memcpy(&val, base+off, sizeof(T));
		off += sizeof(val);
		return true;
	}

	bool readString(std::string & str) {
		unsigned int cch = 0;
		if ( ! readInteger(cch))
			return false;
		return readChars(cch, &str);
	}

	bool readSmallString(std::string & str) {
		unsigned char cch = 0;
		if ( ! readByte(cch))
			return false;
		return readChars(cch, &str);
	}

	bool readString(std::string_view & str) {
		unsigned int cch = 0;
		if ( ! readInteger(cch))
			return false;
		return readChars(cch, str);
	}

	bool readSmallString(std::string_view & str) {
		unsigned char cch = 0;
		if ( ! readByte(cch))
			return false;
		return readChars(cch, str);
	}

	bool readSizeString(std::string & str, bool small_string) { return small_string ? readSmallString(str) : readString(str); }
	bool readSizeString(std::string_view & str, bool small_string) { return small_string ? readSmallString(str) : readString(str); }

	bool readSize(unsigned int & sz, bool small_size) {
		if (small_size) { unsigned char cb = 0;
			bool ret = readByte(cb); sz = cb;
			return ret;
		}
		return readInteger(sz);
	}


	// Consumes a 32-bit length-prefixed number of bytes from this stream
	// making those bytes available to be read from the passed-in stream
	bool readStream(ExprStream & stm) {
		unsigned int cbs = 0;
		if ( ! readInteger(cbs))
			return false;
		return readBytes(cbs, &stm);
	}

	bool readNullableExpr(ExprTree* & tree);
	bool skipNullableExpr(ExprTree::NodeKind & kind);
	//const unsigned char * readExprBytes(ExprTree::NodeKind & kind, unsigned int & size);
	binary_span readExprBytes(ExprTree::NodeKind & kind);

	bool skipStream() {
		unsigned int cbs = 0;
		if ( ! readInteger(cbs))
			return false;
		return readBytes(cbs, nullptr);
	}
	bool skipSmall() {
		unsigned char cbs = 0;
		if ( ! readByte(cbs))
			return false;
		return readBytes(cbs, nullptr);
	}
	bool skipSizeStream(bool small_stream) { return small_stream ? skipSmall() : skipStream(); }
	bool skipComments();
	// returns false on failure (comment is malformed), true on success
	// also returns true if the next token is not a comment
	bool readOptionalComment(std::string_view & comment);
	bool readOptionalComment(std::string & comment, bool append) {
		std::string_view sv;
		if ( ! append) comment.clear();
		if ( ! readOptionalComment(sv)) return false;
		comment.append(sv); return true;
	}

	// peek at the token and ExprTree::NodeKind of the next thing
	// note that not all tokens map to an NodeKind, because there is no kind for pairs and values
	// Kind will be set to EsBadTag
	unsigned char peekKind(ExprTree::NodeKind & kind) {
		unsigned char ct = 0;
		peekByte(ct);
		if (ct >= EsOpBase) {
			kind = (ExprTree::NodeKind)classad::ExprStream::tokToGeneralKind(ct);
		} else {
			kind = (ExprTree::NodeKind)classad::ExprStream::tokToLiteralKind(ct);
		}
		return ct;
	}

};

// A class used to pickled (write) a classad to memory. essentially a structured write-only byte stream
// this class will grow the memory buffer as needed when new data is written to the stream.
//
// When pickling is complete, the pickled data can be read back using the data() and size() methods
// the data stream is NOT null terminated unless the pickling makes it so.
//
class ExprStreamMaker
{
protected:
	unsigned char * base{nullptr};  // pointer to memory buffer owned and used by this class.
	unsigned int    cbAlloc{0}; // current allocation size of the memory buffer
	unsigned int    off{0};   // current used size of the memory buffer

public:

	ExprStreamMaker() = default;
	~ExprStreamMaker() {
		if (base) free(base);
		base = NULL; cbAlloc = 0; off = 0;
	}

	class Mark {
		unsigned int off{0};
		unsigned int siz{0};
	public:
		Mark() = default;
		Mark(unsigned int o, unsigned int s) : off(o), siz(s) {}
		Mark &operator=(const Mark & that) = default; //{ off = that.off; siz = that.siz; return *this; }
		friend class ExprStreamMaker;
		friend class ClassAd;
		unsigned int at() const { return off; }
		unsigned int len() const { return siz; }
		unsigned int end() const { return off+siz; }
	};

	const unsigned char * data() const { return base; }
	unsigned int size() const { return off; }
	binary_span dataspan() const { binary_span spn(base, off); return spn; }

	Mark mark() const { return Mark(off, 0); }
	unsigned int added(const Mark & mk) const { return off - mk.off; }

	// presuming that the mark is a size field for data that follows
	// return an ExprStream that goes from just after the size field
	// for the size indicated
	ExprStream stream_of(const Mark mk) {
		unsigned int size  = 0;
		memcpy(&size, base + mk.off, mk.siz);
		return ExprStream(base + mk.off + mk.siz, size);
	}

	template <typename T> Mark mark(T s) {
		if (off+s > cbAlloc) grow();
		Mark mk(off, sizeof(s));
		off += sizeof(s);
		return mk;
	}

	template <typename T> void updateAt(const Mark& mk, T val) {
		if (mk.off+sizeof(T) <= off && mk.siz >= sizeof(T)) {
			memcpy(base+mk.off, (unsigned char*)&val, sizeof(T));
		}
	}

	// get a buffer to read into at the next position, and advance the position
	// by the size of the buffer.
	unsigned char * appendBuffer(unsigned int s) {
		if (off+s > cbAlloc) grow(cbAlloc+s);
		unsigned char *p = base + off;
		off += s;
		return p;
	}

	void grow(unsigned int min_size=100);
	void clear() { off = 0; }

	template <typename T> void putInteger(T val) {
		if (off + sizeof(T) > cbAlloc) grow();
		memcpy(base+off, &val, sizeof(T));
		off += sizeof(val);
	}

	void putByte(unsigned char b) {
		if (off+1 > cbAlloc) grow();
		base[off] = b;
		++off;
	}

	void putBytes(const unsigned char * p, unsigned int size);
	void putBytes(const char * p, unsigned int size) { putBytes((const unsigned char*)p, size); }

	// put int32 string size followed by string
	void putString(std::string_view str);
	void putString(const std::string & str);
	// put byte string size followed by string
	void putSmallString(std::string_view str);
	void putSmallString(const std::string & str);
	// put expression size followed by expression
	unsigned int putNullableExpr(const ExprTree* tree);

	unsigned int putPair(AttrList::const_iterator itr);
};

#endif // TJ_PICKLE

/// The ClassAd object represents a parsed %ClassAd.
class ClassAd : public ExprTree
{
    /** \mainpage C++ ClassAd API Documentation
     *  Welcome to the C++ ClassAd API Documentation. 
     *  Use the links at the top to navigate. A good link to start with
     *  is the "Class List" link.
     *
     *  If you are unfamiliar with the ClassAd library, look at the 
     *  sample.C file that comes with the ClassAd library. Also check
     *  out the online ClassAd tutorial at: 
     *  http://www.cs.wisc.edu/condor/classad/c++tut.html
     */

  	public:
		/**@name Constructors/Destructor */
		//@{
		/// Default constructor 
		ClassAd () : alternateScope(nullptr), do_dirty_tracking(false), chained_parent_ad(nullptr), parentScope(nullptr) {}

		/** Copy constructor
            @param ad The ClassAd to copy
        */
		ClassAd (const ClassAd &ad);

		/// Destructor
		virtual ~ClassAd ();
		//@}

		/// node type
		virtual NodeKind GetKind (void) const { return CLASSAD_NODE; }

		/**@name Insertion Methods */
		//@{	
		/** Inserts an attribute into the ClassAd.  The setParentScope() method
				is invoked on the inserted expression.
			@param attrName The name of the attribute.  
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool Insert( const std::string& attrName, ExprTree* expr);   // (ignores cache)
		bool InsertLiteral(const std::string& attrName, Literal* lit); // (ignores cache)

		// insert through cache if cache is enabled, otherwise just parse and insert
		// parsing of the rhs expression is done use old ClassAds syntax
		bool InsertViaCache(const std::string& attrName, const std::string & rhs, bool lazy=false);
		bool InsertViaCache(const std::string& attrName, ExprTree* expr); // may free expr and use one from cache instead

#ifdef TJ_PICKLE
		static ClassAd* Make(ExprStream & stm, std::string * label=NULL);
		static ClassAd* MakeViaCache(ExprStream & stm, std::string * label=NULL, std::string * comments=NULL);
		// validate and skip over the next expression in the stream if it is a valid ClassAd or hash of pairs
		// returns the number of bytes read from the stream, or 0 on failure.
		static unsigned int Scan(ExprStream & stm, std::string * label=NULL);

		// update ad from EsHash of pairs
		bool Update(ExprStream & stm);
		bool UpdateViaCache(ExprStream & stm);
		// update attributes matching the whitelist from hash of pairs
		bool Update(ExprStream & stm, const References & whitelist, bool no_advance /*=false*/);

		// worker method for nested ads - ignores the chained parent ad, use one of the other methods for top level classads
		unsigned int Pickle(ExprStreamMaker & stm, const References * whitelist) const;

		// for writing top level classads, does not ignore the parent ad, and it requires a label for the ad
		unsigned int Pickle(ExprStreamMaker & stm, const std::string & label, const References * whitelist, bool flatten) const;

		// two different ways of pickling while observing the parent ads
		// write the parent attributes, then the child attributes (flatten=false)
		unsigned int PickleWithParent(ExprStreamMaker & stm, const References * whitelist) const;
		// write child attributes and visible parent attributes (flatten=true)
		unsigned int PickleFlat(ExprStreamMaker & stm, const References * whitelist) const;
#endif // TJ_PICKLE

		/** Insert an attribute/value into the ClassAd
		 *  @param str A string of the form "Attribute = Value"
		 *  InsertViaCache() is used to do the actual insertion, so the
		 *    value is parsed into an ExprTree in old ClassAds syntax.
		 */
	bool Insert(const char *str);
	bool Insert(const std::string &str);

		/* Insert an attribute/value into the Classad.
		 * The value is parsed into an ExprTree using old ClassAds syntax.
		 */
	bool AssignExpr(const std::string &name, const char *value);

		/** Inserts an attribute into a nested classAd.  The scope expression is
		 		evaluated to obtain a nested classad, and the attribute is 
				inserted into this subclassad.  The setParentScope() method
				is invoked on the inserted expression.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool DeepInsert( ExprTree *scopeExpr, const std::string &attrName, 
				ExprTree *expr );

		/** Inserts an attribute into the ClassAd.  The integer value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The integer value of the attribute.
		*/
		bool InsertAttr( const std::string &attrName,int value );
		bool InsertAttr( const std::string &attrName,long value );
		bool InsertAttr( const std::string &attrName,long long value );
		bool Assign(const std::string &name, int value)
		{ return InsertAttr(name, value); }
		bool Assign(const std::string &name, unsigned int value)
		{ return InsertAttr(name, (long long)value); }
		bool Assign(const std::string &name,long value)
		{ return InsertAttr(name, value); }
		bool Assign(const std::string &name, long long value)
		{ return InsertAttr(name, value); }
		bool Assign(const std::string &name, unsigned long value)
		{ return InsertAttr(name, (long long)value); }
		bool Assign(const std::string &name, unsigned long long value)
		{ return InsertAttr(name, (long long)value); }

		/** Inserts an attribute into a nested classad.  The scope expression 
		 		is evaluated to obtain a nested classad, and the attribute is
		        inserted into this subclassad.  The integer value is
				converted into a Literal expression, and then inserted into
				the nested classad.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param value The integer value of the attribute.
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				int value );
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				long value );
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				long long value );

		/** Inserts an attribute into the ClassAd.  The real value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The real value of the attribute.
            @return true on success, false otherwise
		*/
		bool InsertAttr( const std::string &attrName,double value);
		bool Assign(const std::string &name, float value)
		{ return InsertAttr(name, (double)value); }
		bool Assign(const std::string &name, double value)
		{ return InsertAttr(name, value); }

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The double value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr String representation of the scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
            @param f A multipler for the number.
            @return true on success, false otherwise
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				double value);

		/** Inserts an attribute into the ClassAd.  The boolean value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute. 
			@param value The boolean value of the attribute.
		*/
		bool InsertAttr( const std::string &attrName, bool value );
		bool Assign(const std::string &name, bool value)
		{ return InsertAttr(name, value); }

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The boolean value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.  This string is
				always duplicated internally.
			@param value The string attribute
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName, 
				bool value );

		/** Inserts an attribute into the ClassAd.  The string value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool InsertAttr( const std::string &attrName, const char *value );
		bool InsertAttr( const std::string &attrName, const char * str, size_t len );
		bool Assign(const std::string &name,char const *value)
		{ if (value==NULL) return false; else return InsertAttr( name, value ); }

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The string value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName, 
				const char *value );

		/** Inserts an attribute into the ClassAd.  The string value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool InsertAttr( const std::string &attrName, const std::string &value );
		bool Assign(const std::string &name, const std::string &value)
		{ return InsertAttr(name, value); }

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The string value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName, 
				const std::string &value );
		//@}

		/**@name Lookup Methods */
		//@{
		/** Finds the expression bound to an attribute name.  The lookup only
				involves this ClassAd; scoping information is ignored.
			@param attrName The name of the attribute. char *, or std::string   
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		template<typename StringLikeThing>
		ExprTree * Lookup( const StringLikeThing &name ) const {
			ExprTree *tree;
			AttrList::const_iterator itr;

			itr = attrList.find( name );
			if (itr != attrList.end()) {
				tree = itr->second;
			} else if (chained_parent_ad != NULL) {
				tree = chained_parent_ad->Lookup(name);
			} else {
				tree = NULL;
			}
			return tree;
		}

		ExprTree* LookupExpr(const std::string &name) const
		{ return Lookup( name ); }

		/** Finds the expression bound to an attribute name, ignoring chained parent.
		        Behaves just like Lookup(), except any parent ad chained to this
				ad is ignored.
			@param attrName The name of the attribute.
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *LookupIgnoreChain( const std::string &attrName ) const;

		/** Finds the expression bound to an attribute name.  The lookup uses
				the scoping structure (including <tt>super</tt> attributes) to 
				determine the expression bound to the given attribute name in
				the closest enclosing scope.  The closest enclosing scope is
				also returned.
			@param attrName The name of the attribute.
			@param ad The closest enclosing scope of the returned expression,
				or NULL if no expression was found.
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *LookupInScope(const std::string &attrName,const ClassAd *&ad)const;
		//@}

		/**@name Attribute Deletion Methods */
		//@{
		/** Clears the ClassAd of all attributes. */
		void Clear( );

		/** Deletes the named attribute from the ClassAd.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.  The expression bound to the attribute is deleted.
			@param attrName The name of the attribute to be delete.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool Delete( const std::string &attrName );

		/** Deletes the named attribute from a nested classAd.  The scope
		 		expression is evaluated to obtain a nested classad, and the
				attribute is then deleted from this ad.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.  The expression bound to the attribute is deleted.
			@param scopeExpr String representation of the scope expression.
			@param attrName The name of the attribute to be delete.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool DeepDelete( const std::string &scopeExpr, const std::string &attrName );

		/** Deletes the named attribute from a nested classAd.  The scope
		 		expression is evaluated to obtain a nested classad, and the
				attribute is then deleted from this ad.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.  The expression bound to the attribute is deleted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute to be delete.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool DeepDelete( ExprTree *scopeExpr, const std::string &attrName );
	
		/** Similar to Delete, but the expression is returned rather than 
		  		deleted from the classad.
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *Remove( const std::string &attrName );

		/** Similar to DeepDelete, but the expression is returned rather than 
		  		deleted from the classad.
			@param scopeExpr String representation of the scope expression
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *DeepRemove( const std::string &scopeExpr, const std::string &attrName );

		/** Similar to DeepDelete, but the expression is returned rather than 
		  		deleted from the classad.
			@param scopeExpr The scope expression
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *DeepRemove( ExprTree *scopeExpr, const std::string &attrName );
		//@}

		/**@name Evaluation Methods */
		//@{
		/** Evaluates expression bound to an attribute.
			@param attrName The name of the attribute in the ClassAd.
			@param result The result of the evaluation.
		*/
		bool EvaluateAttr( const std::string& attrName, Value &result, Value::ValueType mask=Value::ValueType::SAFE_VALUES ) const;

		/** Evaluates an expression.
			@param buf Buffer containing the external representation of the
				expression.  This buffer is parsed to yield the expression to
				be evaluated.
			@param result The result of the evaluation.
			@return true if the operation succeeded, false otherwise.
		*/
		bool EvaluateExpr( const std::string& buf, Value &result ) const;

		/** Evaluates an expression.  If the expression doesn't already live in
				this ClassAd, the setParentScope() method must be called
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
		*/
		bool EvaluateExpr( const ExprTree* expr, Value &result, Value::ValueType mask=Value::ValueType::SAFE_VALUES ) const;

		/* Evaluates a loose expression in the context of a single ad (or nullptr) without mutating the expression
		 *   Note that since this function cannot call setParentScope() on tree, the tree cannot contain a nested ad that
		 *   with attribute references that are expected to resolve against attributes of the ad.
		 *   i.e.  when expr is [foo = RequestCpus;].foo, foo will evaluate to 'undefined' rather than looking up RequestCpus in the ad.
		 */
		static bool EvaluateExpr(const ClassAd * ad, const ExprTree * tree, Value & val, Value::ValueType mask=Value::ValueType::SAFE_VALUES) {
			EvalState state;
			state.SetScopes(ad);
			if ( ! tree->Evaluate(state, val)) return false;
			if ( ! val.SafetyCheck(state, mask)) return false;
			return true;
		}

		/** Evaluates an expression, and returns the significant subexpressions
				encountered during the evaluation.  If the expression doesn't 
				already live in this ClassAd, call the setParentScope() method 
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
			@param sig The significant subexpressions of the evaluation.
		*/
		bool EvaluateExpr( const ExprTree* expr, Value &result, ExprTree *&sig) const;

		/** Evaluates an attribute to an integer.
			@param attr The name of the attribute.
			@param intValue The value of the attribute.
			If the type of intValue is smaller than a long long, the
			value may be truncated.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool EvaluateAttrInt( const std::string &attr, int& intValue ) const;
		bool EvaluateAttrInt( const std::string &attr, long& intValue ) const;
		bool EvaluateAttrInt( const std::string &attr, long long& intValue ) const;

		/** Evaluates an attribute to a real.
			@param attr The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a real, false otherwise.
		*/
		bool EvaluateAttrReal( const std::string &attr, double& realValue )const;

		/** Evaluates an attribute to an integer. If the attribute evaluated to 
				a real, it is truncated to an integer.
				If the value is a boolean, it is converted to 0 (for False)
				or 1 (for True).
			@param attr The name of the attribute.
			@param intValue The value of the attribute.
			If the type of intValue is smaller than a long long, the
			value may be truncated.
			@return true if attrName evaluated to an number, false otherwise.
		*/
		bool EvaluateAttrNumber( const std::string &attr, int& intValue ) const;
		bool EvaluateAttrNumber( const std::string &attr, long& intValue ) const;
		bool EvaluateAttrNumber( const std::string &attr, long long& intValue ) const;
		bool LookupInteger(const std::string &name, int &value) const
		{ return EvaluateAttrNumber(name, value); }
		bool LookupInteger(const std::string &name, long &value) const
		{ return EvaluateAttrNumber(name, value); }
		bool LookupInteger(const std::string &name, long long &value) const
		{ return EvaluateAttrNumber(name, value); }

		/** Evaluates an attribute to a real.  If the attribute evaluated to an 
				integer, it is promoted to a real.
				If the value is a boolean, it is converted to 0.0 (for False)
				or 1.0 (for True).
			@param attr The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a number, false otherwise.
		*/
		bool EvaluateAttrNumber(const std::string &attr,double& realValue) const;

		// Should really be called LookupDouble, but I'm afraid
		// to rename this everyone right now
		bool LookupFloat(const std::string &name, double &value) const
		{ return EvaluateAttrNumber(name, value); }

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attr The name of the attribute.
			@param buf The buffer for the string value.
			@param len Size of buffer
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const std::string &attr, char *buf, int len) 
				const;
		bool LookupString(const std::string &name, char *value, int max_len) const
		{ return EvaluateAttrString( name, value, max_len ); }
		bool LookupString(const std::string &name, char **value) const
		{
			std::string sval;
			bool rc = EvaluateAttrString(name, sval);
			if ( rc ) *value = strdup(sval.c_str());
			return rc;
		}

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attr The name of the attribute.
			@param buf The buffer for the string value.
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const std::string &attr, std::string &buf ) const;
		bool LookupString(const std::string &name, std::string &value) const
		{ return EvaluateAttrString( name, value ); }

		/** Evaluates an attribute to a boolean.
			@param attr The name of the attribute.
			@param boolValue The value of the attribute.
			@return true if attrName evaluated to a boolean value, false 
				otherwise.
		*/
		bool EvaluateAttrBool( const std::string &attr, bool& boolValue ) const;

		/** Evaluates an attribute to a boolean. If old ClassAd semantics
				are enabled, then numerical values will be converted to the
				appropriate boolean value.
			@param attr The name of the attribute.
			@param boolValue The value of the attribute.
			@return true if attrName evaluated to a boolean value, false 
				otherwise.
		*/
		bool EvaluateAttrBoolEquiv( const std::string &attr, bool& boolValue ) const;
		bool LookupBool(const std::string &name, bool &value) const
		{ return EvaluateAttrBoolEquiv(name, value); }

		/**@name STL-like Iterators */
		//@{

		/** Define an iterator we can use on a ClassAd */
		typedef AttrList::iterator iterator;

		/** Define a constatnt iterator we can use on a ClassAd */
		typedef AttrList::const_iterator const_iterator;

		/** Returns an iterator pointing to the beginning of the
			attribute/value pairs in the ClassAd */
		iterator begin() { return attrList.begin(); }

		/** Returns a constant iterator pointing to the beginning of the
			attribute/value pairs in the ClassAd */
		const_iterator begin() const { return attrList.begin(); }

		/** Returns aniterator pointing past the end of the
			attribute/value pairs in the ClassAd */
		iterator end() { return attrList.end(); }

		/** Returns a constant iterator pointing past the end of the
			attribute/value pairs in the ClassAd */
		const_iterator end() const { return attrList.end(); }

        /** Return an interator pointing to the attribute with a particular name.
         */
		iterator find(std::string const& attrName);

        /** Return a constant interator pointing to the attribute with a particular name.
         */
		const_iterator find(std::string const& attrName) const;

        /** Return the number of attributes at the root level of this ClassAd.
         */
        int size(void) const { return (int)attrList.size(); }
		//@}

		void rehash(size_t s) { attrList.rehash(s);}
		iterator erase(iterator i) { 
			return attrList.erase(i);
		}
		/** Deconstructor to get the components of a classad
		 * 	@param vec A vector of (name,expression) pairs which are the
		 * 		attributes of the classad
		 */
		void GetComponents( std::vector< std::pair< std::string, ExprTree *> > &vec ) const;
		void GetComponents( std::vector< std::pair< std::string, ExprTree *> > &vec, const References &whitelist ) const;

        /** Make sure everything in the ad is in this ClassAd.
         *  This is different than CopyFrom() because we may have many 
         *  things that the ad doesn't have: we just ensure that everything we 
         *  import everything from the other ad. This could be called Merge().
         *  @param ad The ad to copy attributes from.
         */
		bool Update( const ClassAd& ad );	

        /** Modify this ClassAd in a specific way
         *  Ad is a ClassAd that looks like:
         *  [ Context = expr;    // Sub-ClassAd to operate on
         *    Replace = classad; // ClassAd to Update() replace context
         *    Updates = classad; // ClassAd to merge into context (via Update())
         *    Deletes = {a1, a2};  // A list of attribute names to delete from the context
         *  @param ad is a description of how to modify this ClassAd
         */
		void Modify( ClassAd& ad );

        /** Makes a deep copy of the ClassAd.
           	@return A deep copy of the ClassAd, or NULL on failure.
         */
		virtual ExprTree* Copy( ) const;

        /** Make a deep copy of the ClassAd, via the == operator. 
         */
		ClassAd &operator=(const ClassAd &rhs);

		ClassAd &operator=(ClassAd &&rhs)  noexcept {
			this->do_dirty_tracking = rhs.do_dirty_tracking;
			this->chained_parent_ad = rhs.chained_parent_ad;
			this->alternateScope = rhs.alternateScope;

			this->dirtyAttrList = std::move(rhs.dirtyAttrList);
			this->attrList = std::move(rhs.attrList);

			return *this;
		}
        /** Fill in this ClassAd with the contents of the other ClassAd.
         *  This ClassAd is cleared of its contents before the copy happens.
         *  @return true if the copy succeeded, false otherwise.
         */
		bool CopyFrom( const ClassAd &ad );

        /** Is this ClassAd the same as the tree?
         *  Two ClassAds are identical if they have the same
         *  number of elements, and each is the SameAs() the other. 
         *  This is a deep comparison.
         *  @return true if it is the same, false otherwise
         */
        virtual bool SameAs(const ExprTree *tree) const;

        /** Are the two ClassAds the same?
         *  Uses SameAs() to decide if they are the same. 
         *  @return true if they are, false otherwise.
         */
        friend bool operator==(ClassAd &list1, ClassAd &list2);

		/** Flattens (a partial evaluation operation) the given expression in 
		  		the context of the classad.
			@param expr The expression to be flattened.
			@param val The value after flattening, if the expression was 
				completely flattened.  This value is valid if and only if	
				fexpr is NULL.
			@param fexpr The flattened expression tree if the expression did
				not flatten to a single value, and NULL otherwise.
			@return true if the flattening was successful, and false otherwise.
		*/
		bool Flatten( const ExprTree* expr, Value& val, ExprTree *&fexpr )const;

		bool FlattenAndInline( const ExprTree* expr, Value& val,	// NAC
							   ExprTree *&fexpr )const;				// NAC
		
        /** Return a list of attribute references in the expression that are not 
         *  contained within this ClassAd.
         *  @param tree The ExprTree for the expression that has references that you are
         *  wish to know about. 
         *  @param refs The list of references
         *  @param fullNames true if you want full names (like other.foo)
         *  @return true on success, false on failure. 
         */
		bool GetExternalReferences( const ExprTree *tree, References &refs, bool fullNames ) const;

        /** Return a list of attribute references in the expression that are not 
         *  contained within this ClassAd.
         *  @param tree The ExprTree for the expression that has references that you are
         *  wish to know about. 
         *  @param refs The list of references
         *  @return true on success, false on failure. 
         */
		bool GetExternalReferences(const ExprTree *tree, PortReferences &refs) const;
		//@}


        /** Return a list of attribute references in the expression that are
         *  contained within this ClassAd.
         *  @param tree The ExprTree for the expression that has references 
         *      that you wish to know about. 
         *  @param refs The list of references
         *  @param fullNames ignored
         *  @return true on success, false on failure. 
         */
        bool GetInternalReferences( const ExprTree *tree, References &refs, bool fullNames) const;

		/**@name Chaining functions */
        //@{
        /** Chain this ad to the parent ad.
         *  After chaining, any attribute we look for that is not
         *  in this ad will be looked for in the parent ad. This is
         *  a simple form of compression: many ads can be linked to a 
         *  parent ad that contains common attributes between the ads. 
         *  If an attribute is in both this ad and the parent, a lookup
         *  will only show it in the parent. If we make any modifications to
         *  this ad, it will not affect the parent. 
         *  @param new_chain_parent_ad the parent ad we are chained too.
         */
	    void		ChainToAd(ClassAd *new_chain_parent_ad);
		
		/** If there is a chained parent remove redundant entries.
		 */
		int 		PruneChildAd();

		/** If there is a chained parent remove this attribute from the child ad only
		 *  if there is no chained parent ad, this function does nothing - you should use the Delete method in that case
		 * @param if_child_matches, remove the attribute only if value in the child ad matches the value in the chained parent ad
		 */
		bool 		PruneChildAttr(const std::string & attrName, bool if_child_matches=true);

        /** If we are chained to a parent ad, remove the chain. 
         */
		void		Unchain(void);

		/** Return a pointer to the parent ad.
		 */
		ClassAd *   GetChainedParentAd(void);
		const ClassAd *   GetChainedParentAd(void) const;

		/** Fill in this ClassAd with the contents of the other ClassAd,
		 *  including any attributes from the other ad's chained parent.
		 *  Any previous contents of this ad are cleared.
		 *  @return true if the copy succeeded, false otherwise.
		 */
		bool CopyFromChain( const ClassAd &ad );

		/** Insert all attributes from the other ad and its chained
		 *  parents into this ad, but do not clear out existing
		 *  contents of this ad before doing so.
		 *  @return true if the operation succeeded, false otherwise.
		 */
		bool UpdateFromChain( const ClassAd &ad );
        //@}

		/**@name Dirty Tracking */
        //@{
		/** enable or disable dirty tracking for this ClassAd
		 *  and return whether dirty track was previously enabled or disabled.
		 */
		bool       SetDirtyTracking(bool enable) { bool was_enabled = do_dirty_tracking; do_dirty_tracking = enable; return was_enabled; }
        /** Turn on dirty tracking for this ClassAd. 
         *  If tracking is on, every insert will label the attribute that was inserted
         *  as dirty. Dirty tracking is always turned off during Copy() and
         *  CopyFrom().
         */ 
		void        EnableDirtyTracking(void)  { do_dirty_tracking = true;  }
        /** Turn off ditry tracking for this ClassAd.
         */
		void        DisableDirtyTracking(void) { do_dirty_tracking = false; }
        /** Mark all attributes in the ClassAd as not dirty
         */
		void		ClearAllDirtyFlags(void);
        /** Mark a particular attribute as dirty
         * @param name The attribute name
         */

		void        MarkAttributeDirty(const std::string &name) {
			if (do_dirty_tracking) dirtyAttrList.insert(name);
		}

        /** Mark a particular attribute as not dirty 
         * @param name The attribute name
         */
		void        MarkAttributeClean(const std::string &name);
        /** Return true if an attribute is dirty
         *  @param name The attribute name
         *  @return true if the attribute is dirty, false otherwise
         */
		bool        IsAttributeDirty(const std::string &name) const;

		/* Needed for backward compatibility
		 * Remove it the next time we have to bump the ClassAds SO version.
		 */
		bool        IsAttributeDirty(const std::string &name) {return ((const ClassAd*)this)->IsAttributeDirty(name);}

		typedef DirtyAttrList::iterator dirtyIterator;
        /** Return an interator to the first dirty attribute so all dirty attributes 
         * can be iterated through.
         */
		dirtyIterator dirtyBegin() { return dirtyAttrList.begin(); }
        /** Return an iterator past the last dirty attribute
         */
		dirtyIterator dirtyEnd() { return dirtyAttrList.end(); }
        //@}

		/* This data member is intended for transitioning Condor from
		 * old to new ClassAds. It allows unscoped attribute references
		 * in expressions that can't be found in the local scope to be
		 * looked for in an alternate scope. In Condor, the alternate
		 * scope is the Target ad in matchmaking.
		 * Expect alternateScope to be removed from a future release.
		 */
		ClassAd *alternateScope;

		virtual const ClassAd *GetParentScope( ) const { return( parentScope ); }

		static bool _GetExternalReferences( const ExprTree *, const ClassAd *,
					EvalState &, References&, bool fullNames );

  	private:
		friend 	class AttributeReference;
		friend 	class ExprTree;
		friend 	class EvalState;


		bool _GetExternalReferences( const ExprTree *, const ClassAd *, 
					EvalState &, PortReferences& ) const;

        bool _GetInternalReferences(const ExprTree *expr, const ClassAd *ad,
            EvalState &state, References& refs, bool fullNames) const;

		ClassAd *_GetDeepScope( const std::string& ) const;
		ClassAd *_GetDeepScope( ExprTree * ) const;

		virtual void _SetParentScope( const ClassAd* p );
		virtual bool _Evaluate( EvalState& , Value& ) const;
		virtual bool _Evaluate( EvalState&, Value&, ExprTree*& ) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
	
		int LookupInScope( const std::string&, ExprTree*&, EvalState& ) const;
		AttrList	  attrList;
		DirtyAttrList dirtyAttrList;
		bool          do_dirty_tracking;
		ClassAd       *chained_parent_ad;
		const ClassAd *parentScope;
};

} // classad

#endif//__CLASSAD_CLASSAD_H__
