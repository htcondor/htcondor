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


typedef std::map<std::string, classad::Value::ValueType, classad::CaseIgnLTStr> Filter;

// The caller owns (is responsible for delete'ing) the returned ClassAd.
classad::ClassAd *
filterClassAd( const classad::ClassAd & ad, const Filter & filter ) {
    // The ClassAd iterator isn't sorted, which set_intersection() requires.
    classad::References attributes;
    sGetAdAttrs( attributes, ad );

    std::vector<std::string> intersection;
    std::ranges::set_intersection(
        attributes,
        filter | std::ranges::views::keys,
        std::back_inserter( intersection ),
        classad::CaseIgnLTStr()
    );


    if( intersection.empty() ) { return NULL; }

    classad::ClassAd * filteredAd = new classad::ClassAd();
    for( const auto & attribute : intersection ) {
        classad::Value v;
        ExprTree * e = ad.Lookup( attribute );
// We can do this without evaluating if follow the example of compat classad's
// ExprTreeIsLiteral*(); if e->GetKind() is CLASSAD_NODE or EXPR_LIST_NODE,
// that's the type as far as we're presently concerned.  If it's LITERAL_NODE,
// ExprTreeIsLiteral() actually returns the classad::Value without evaluating,
// which we can of course then look up directly.
        if( e->Evaluate(v) ) {
                if( v.GetType() == filter.at(attribute) ) {
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
filterTransferErrorData( const classad::ClassAd & ad ) {
    using namespace classad;

    Filter LegalAttributes {
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

    return filterClassAd( ad, LegalAttributes );
}


// The caller owns (is responsible for delete'ing) the returned ClassAd.
classad::ClassAd *
filterPluginResults( const classad::ClassAd & ad ) {
    using namespace classad;

    Filter LegalAttributes {
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
        { "DeveloperData",          Value::CLASSAD_VALUE },
        { "TransferData",           Value::CLASSAD_VALUE },
    };

    classad::ClassAd * filteredAd = filterClassAd( ad, LegalAttributes );

    // Additionally, filter the TransferErrorData attribute.
    ExprTree * e = filteredAd->Lookup(" TransferErrorData" );
    if( e != NULL ) {
        classad::ClassAd * transferErrorDataAd = NULL;
        assert( e->isClassad( & transferErrorDataAd ) );
        classad::ClassAd * filteredTEDAd = filterTransferErrorData( * transferErrorDataAd );
        filteredAd->Insert( "TransferErrorData", filteredTEDAd );
    }

    return filteredAd;
}
