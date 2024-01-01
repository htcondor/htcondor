#include "condor_common.h"
#include "condor_debug.h"

#include "classad/classad.h"
#include "filter.h"
#include "compat_classad.h"

#include <map>
#include <string>
#include <vector>
#include <algorithm>


template<typename I>
class FirstIterator {
  public:
    FirstIterator( const I & iter ) { i = iter; }

    FirstIterator & operator++() { ++i; return * this; }

    FirstIterator   operator++(int)  {
        FirstIterator rv = *this;
        ++i;
        return rv;
    }

    I::value_type::first_type operator*() { return i->first; }

    bool operator == (const FirstIterator & o) { return i == o.i; }
    bool operator != (const FirstIterator & o) { return i != o.i; }

  private:
    I i;
};


// The caller is responsible for delete'ing the return ClassAd.
classad::ClassAd *
filterPluginResults( const classad::ClassAd & ad ) {
	using namespace classad;

	std::map<std::string, classad::Value::ValueType> LegalTransferStats {
		{ "TransferSuccess",        Value::BOOLEAN_VALUE },
		{ "TransferError",          Value::STRING_VALUE },
		{ "TransferProtocol",       Value::STRING_VALUE },
		{ "TransferType",           Value::STRING_VALUE },
		{ "TransferFileName",       Value::STRING_VALUE },
		{ "TransferFileBytes",      Value::INTEGER_VALUE },
		{ "TransferTotalBytes",     Value::INTEGER_VALUE },
		{ "TransferStartTime",      Value::INTEGER_VALUE },
		{ "TransferEndTime",        Value::INTEGER_VALUE },
		{ "ConnectionTimeSeconds",  Value::INTEGER_VALUE },
		{ "TransferURL",            Value::STRING_VALUE },
		{ "TransferErrorData",      Value::CLASSAD_VALUE }
	};


    // Although FirstIterator() _works_ on ad.begin() and ad.end(),
    // the algorithm fails because the ClassAd iterator isn't sorted.
    classad::References attributes;
    sGetAdAttrs( attributes, ad );

	std::vector<std::string> intersection;
	std::set_intersection(
		attributes.begin(), attributes.end(),
		FirstIterator(LegalTransferStats.begin()),
		FirstIterator(LegalTransferStats.end()),
		std::back_inserter( intersection ),
		CaseIgnLTStr()
	);


	if( intersection.empty() ) { return NULL; }
	classad::ClassAd * filteredAd = new classad::ClassAd();
	for( const auto & attribute : intersection ) {
		// FIXME: Filter the internal structure of 'TransferErrorData'.
		filteredAd->Insert( attribute, ad.Lookup( attribute )->Copy() );
	}
	return filteredAd;
}
