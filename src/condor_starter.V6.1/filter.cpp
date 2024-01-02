#include "condor_common.h"
#include "condor_debug.h"

#include "classad/classad.h"
#include "filter.h"
#include "compat_classad.h"

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <ranges>


// The caller is responsible for delete'ing the return ClassAd.
classad::ClassAd *
filterPluginResults( const classad::ClassAd & ad ) {
    using namespace classad;

    std::map<std::string, classad::Value::ValueType, CaseIgnLTStr> LegalTransferStats {
        { "TransferSuccess",        Value::BOOLEAN_VALUE },
        { "TransferError",          Value::STRING_VALUE },
        { "TransferProtocol",       Value::STRING_VALUE },
        { "TransferType",           Value::STRING_VALUE },
        { "TransferFileName",       Value::STRING_VALUE },
        { "TransferFileBytes",      Value::INTEGER_VALUE },
        { "TransferTotalBytes",     Value::INTEGER_VALUE },
        { "TransferStartTime",      Value::INTEGER_VALUE },
        { "TransferEndTime",        Value::INTEGER_VALUE },
        { "ConnectionTimeSeconds",  Value::REAL_VALUE },
        { "TransferURL",            Value::STRING_VALUE },
        { "TransferErrorData",      Value::CLASSAD_VALUE },
    };


    // The ClassAd iterator isn't sorted, which set_intersection() requires.
    classad::References attributes;
    sGetAdAttrs( attributes, ad );

    std::vector<std::string> intersection;
    std::ranges::set_intersection(
        attributes,
        LegalTransferStats | std::ranges::views::keys,
        std::back_inserter( intersection ),
        CaseIgnLTStr()
    );


    if( intersection.empty() ) { return NULL; }

    classad::ClassAd * filteredAd = new classad::ClassAd();
    for( const auto & attribute : intersection ) {
        classad::Value v;
        ExprTree * e = ad.Lookup( attribute );
        if( e->Evaluate(v) ) {
                // FIXME: Filter the internal structure of 'TransferErrorData'.
                if( v.GetType() == LegalTransferStats[attribute] ) {
                    filteredAd->Insert( attribute, e->Copy() );
                } else {
                    dprintf( D_NEVER, "illegal type %d for attribute %s\n", (int)v.GetType(), attribute.c_str() );

                    std::string buffer;
                    classad::ClassAdUnParser caup;
                    caup.Unparse(buffer, e);
                    dprintf( D_NEVER, "expression was '%s'\n", buffer.c_str() );
                }
        } else {
            dprintf( D_NEVER, "failed to evaluate attribute %s\n", attribute.c_str() );

            std::string buffer;
            classad::ClassAdUnParser caup;
            caup.Unparse(buffer, e);
            dprintf( D_NEVER, "expression was '%s'\n", buffer.c_str() );
        }
    }

    return filteredAd;
}
