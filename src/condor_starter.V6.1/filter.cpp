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

//
// Excepting the special case, these two function are identical, so
// they should be refactored together.  Maybe by having a callback
// for each CLASSAD_VALUE (with the extracted ClassAd and holding
// attribute name as arguments)?
//


// The caller owns (is responsible for delete'ing) the returned ClassAd.
classad::ClassAd *
filterTransferErrorData( const classad::ClassAd & ad ) {
    using namespace classad;

    std::map<std::string, classad::Value::ValueType, CaseIgnLTStr> LegalAttributes {
        { "DeveloperData",                  Value::CLASSAD_VALUE },
        { "IntermediateServerErrorType",    Value::STRING_VALUE },
        { "IntermediateServer",             Value::STRING_VALUE },
        { "ErrorType",                      Value::STRING_VALUE },
        { "PluginVersion",                  Value::STRING_VALUE },
        { "PluginLaunched",                 Value::BOOLEAN_VALUE },
        { "FailedName",                     Value::STRING_VALUE },
        { "FailureType",                    Value::STRING_VALUE },
        { "FailedServer",                   Value::STRING_VALUE },
        { "ShouldRefresh" ,                 Value::BOOLEAN_VALUE },
        { "ErrorCode",                      Value::INTEGER_VALUE },
        { "ErrorSrring",                    Value::STRING_VALUE },
    };


    // The ClassAd iterator isn't sorted, which set_intersection() requires.
    classad::References attributes;
    sGetAdAttrs( attributes, ad );

    std::vector<std::string> intersection;
    std::ranges::set_intersection(
        attributes,
        LegalAttributes | std::ranges::views::keys,
        std::back_inserter( intersection ),
        CaseIgnLTStr()
    );


    if( intersection.empty() ) { return NULL; }

    classad::ClassAd * filteredAd = new classad::ClassAd();
    for( const auto & attribute : intersection ) {
        classad::Value v;
        ExprTree * e = ad.Lookup( attribute );
        if( e->Evaluate(v) ) {
                if( v.GetType() == LegalAttributes[attribute] ) {
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


// The caller owns (is responsible for delete'ing) the returned ClassAd.
classad::ClassAd *
filterPluginResults( const classad::ClassAd & ad ) {
    using namespace classad;

    std::map<std::string, classad::Value::ValueType, CaseIgnLTStr> LegalAttributes {
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
        LegalAttributes | std::ranges::views::keys,
        std::back_inserter( intersection ),
        CaseIgnLTStr()
    );


    if( intersection.empty() ) { return NULL; }

    classad::ClassAd * filteredAd = new classad::ClassAd();
    for( const auto & attribute : intersection ) {
        classad::Value v;
        ExprTree * e = ad.Lookup( attribute );
        if( e->Evaluate(v) ) {
                if( v.GetType() == LegalAttributes[attribute] ) {
                    if( attribute == "TransferErrorData" ) {
                        ClassAd * tedAd = NULL;
                        ASSERT(v.IsClassAdValue(tedAd));
                        ClassAd * filteredTEDAd = filterTransferErrorData(* tedAd);
                        if( filteredTEDAd != NULL ) {
                            filteredAd->Insert( attribute, filteredTEDAd );
                        }
                    } else {
                        filteredAd->Insert( attribute, e->Copy() );
                    }
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
