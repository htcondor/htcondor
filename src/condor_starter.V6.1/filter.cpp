#include "condor_common.h"
#include "condor_debug.h"

#include "classad/classad.h"
#include "filter.h"
#include "compat_classad.h"
#include "compat_classad_util.h"

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


    classad::ClassAd * filteredAd = new classad::ClassAd();
    for( const auto & attribute : intersection ) {
        classad::Value v;
        ExprTree * e = ad.Lookup( attribute );
        if( ExprTreeIsLiteral(e, v) && v.GetType() == filter.at(attribute) ) {
            filteredAd->Insert( attribute, e->Copy() );
        } else if( e->isClassad() &&
                   classad::Value::CLASSAD_VALUE == filter.at(attribute) ) {
            filteredAd->Insert( attribute, e->Copy() );
        } else if( e->GetKind() == ExprTree::EXPR_LIST_NODE &&
                   classad::Value::LIST_VALUE == filter.at(attribute) ) {
            filteredAd->Insert( attribute, e->Copy() );
        } else {
            std::string buffer;
            classad::ClassAdUnParser caup;
            caup.Unparse(buffer, e);

            dprintf( D_NEVER, "Attribute '%s' had value '%s' with wrong type, filtering.\n", attribute.c_str(), buffer.c_str() );
        }
    }

    return filteredAd;
}


// The caller owns (is responsible for delete'ing) the returned ClassAd.
classad::ClassAd *
filterTransferErrorData( const classad::ClassAd & ad ) {
    using namespace classad;

    Filter LegalAttributes {
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
        { "ErrorString",                    Value::STRING_VALUE },
        // Reserved for plug-in developers.
        { "DeveloperData",                  Value::CLASSAD_VALUE },
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
        // A list of ClassAds, one per transfer attempt.
        { "TransferErrorData",      Value::LIST_VALUE },
        // Reserved for plug-in developers.
        { "DeveloperData",          Value::CLASSAD_VALUE },
        // Reserved for future expansion.
        { "TransferData",           Value::LIST_VALUE },
    };

    classad::ClassAd * filteredAd = filterClassAd( ad, LegalAttributes );

    // Additionally, filter the TransferErrorData attribute.
    ExprTree * e = filteredAd->Lookup( "TransferErrorData" );
    if( e != NULL ) {
        if( e->GetKind() != ExprTree::EXPR_LIST_NODE ) {
            EXCEPT("TransferErrorData filtered in but not a list");
        }

        ExprList * list = dynamic_cast<ExprList *>(e);
        if( list == NULL ) {
            EXCEPT("Failed to cast EXPR_LIST_NODE to ExprList");
        }


        std::vector<ExprTree *> exprs;
        list->GetComponents(exprs);

        std::vector<ExprTree *> filteredExprs;
        for( const ExprTree * expr : exprs ) {
            classad::ClassAd * transferErrorDataAd = nullptr;
            if(! expr->isClassad( & transferErrorDataAd )) { continue; }
            classad::ClassAd * filteredTEDAd = filterTransferErrorData( * transferErrorDataAd );
            filteredExprs.push_back(filteredTEDAd);
        }

        ExprList * filteredList = new ExprList( filteredExprs );
        filteredAd->Insert( "TransferErrorData", filteredList );
    }

    return filteredAd;
}
