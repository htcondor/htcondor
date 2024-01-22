#include "condor_common.h"
#include "condor_debug.h"

#include "classad/classad.h"
#include "../condor_starter.V6.1/filter.h"
#include "compat_classad_util.h"

#include <map>
#include <string>


// The public API is just filterPluginResults(), but of course that doesn't
// if filterTransferErrorData() doens't work, which doesn't work if
// filterClassAd() doesn't work.
typedef std::map<std::string, classad::Value::ValueType, classad::CaseIgnLTStr> Filter;
extern classad::ClassAd * filterClassAd( const classad::ClassAd & ad, const Filter & filter );
extern classad::ClassAd * filterTransferErrorData( const classad::ClassAd & ad );


int
main( int, char ** ) {
    using namespace classad;

    Filter legalAttributes {
        { "int-value",      Value::INTEGER_VALUE },
        { "bool-value",     Value::BOOLEAN_VALUE },
        { "real-value",     Value::REAL_VALUE },
        { "string-value",   Value::STRING_VALUE },
        { "classad-value",  Value::CLASSAD_VALUE },
        { "list-value",     Value::LIST_VALUE }
    };


    // Does filterClassAd() filter-in correctly?
    ClassAd sourceAd;
    sourceAd.InsertAttr( "int-value", 7 );
    sourceAd.InsertAttr( "bool-value", true );
    sourceAd.InsertAttr( "real-value", 3.141 );
    sourceAd.InsertAttr( "string-value", "eight" );
    ClassAd * subAd = new ClassAd();
    sourceAd.Insert( "classad-value", subAd );
    subAd = NULL; // sourceAd owns this now, don't re-use it.
    sourceAd.AssignExpr( "list-value", "{7, 8, 9}" );

    auto * filteredAd = filterClassAd( sourceAd, legalAttributes );
    for( const auto & [key, value] : legalAttributes ) {
        ExprTree * e = filteredAd->Lookup(key);
        if(e == NULL) {
            fprintf( stderr, "FAILURE: Attribute '%s' wrongly filtered out\n", key.c_str() );
            return 1;
        }
    }
    delete filteredAd;
    filteredAd = NULL;


    // Does filterClassAd() filter-out correctly?
    sourceAd.Clear();
    sourceAd.InsertAttr( "int-value", 3.141 );
    sourceAd.InsertAttr( "bool-value", "maybe" );
    sourceAd.InsertAttr( "real-value", "pi" );
    sourceAd.InsertAttr( "string-value", 7 );
    sourceAd.InsertAttr( "classad-value", "classad-value" );
    sourceAd.InsertAttr( "list-value", false );

    filteredAd = filterClassAd( sourceAd, legalAttributes );
    if( filteredAd->size() != 0 ) {
        fprintf( stderr, "FAILURE: Some attribute wrongly filtered in\n" );
        return 1;
    }
    delete filteredAd;


    // Does filterClassAd() filter in and out simultaneously?
    sourceAd.Clear();
    sourceAd.InsertAttr( "int-value", 3.141 );
    sourceAd.InsertAttr( "bool-value", true );
    sourceAd.InsertAttr( "real-value", "pi" );
    sourceAd.InsertAttr( "string-value", "eight" );
    subAd = new ClassAd();
    sourceAd.Insert( "classad-value", subAd );
    subAd = NULL; // sourceAd owns this now, don't re-use it.
    sourceAd.InsertAttr( "list-value", false );

    filteredAd = filterClassAd( sourceAd, legalAttributes );
    if( filteredAd->size() != 3 ) {
        fprintf( stderr, "FAILURE: wrong attribute count\n" );
        return 1;
    }
    std::vector< std::string > presentAttributes { "bool-value", "string-value", "classad-value" };
    std::vector< std::string > absentAttributes { "int-value", "real-value", "list-value" };
    for( const auto & attr : presentAttributes ) {
        if( filteredAd->Lookup(attr) == NULL ) {
            fprintf( stderr, "FAILURE: missing expected attribute '%s'\n", attr.c_str() );
            return 1;
        }
    }
    for( const auto & attr : absentAttributes ) {
        if( filteredAd->Lookup(attr) != NULL ) {
            fprintf( stderr, "FAILURE: expected absent attribute '%s' present\n", attr.c_str() );
            return 1;
        }
    }
    delete filteredAd;


    // If so, let's not test the filters in filterPluginResults() and
    // filterTransferErrorData(); there's little to be gained there, and
    // contents of those filters is likely to change.  Instead, let's
    // make sure that filterPluginResults() calls filterTransferErrorData()
    // properly and call it a day.
    sourceAd.Clear();
    sourceAd.AssignExpr( "TransferErrorData",
        "{ [ ErrorType = \"Contact\"; ], [ BadAttr = \"Transfer\"; ] }"
    );
    filteredAd = filterPluginResults( sourceAd );
    if( filteredAd->size() != 1 ) {
        fprintf( stderr, "FAILURE: wrong attribute count (#2)\n" );
        return 1;
    }
    ExprTree * e = filteredAd->Lookup( "TransferErrorData" );
    if(! e) {
        fprintf( stderr, "'TransferErrorData' incorrectly filtered out.\n" );
        return 1;
    }
    if( e->GetKind() != ExprTree::EXPR_LIST_NODE ) {
        fprintf( stderr, "'TransferErrorData' not a list.\n" );
        return 1;
    }
    ExprList * list = dynamic_cast<ExprList *>(e);
    std::vector<ExprTree *> ads;
    list->GetComponents(ads);
    ClassAd * first = dynamic_cast<ClassAd *>(ads[0]);
    if(! (first != NULL && first->size() == 1)) {
        fprintf( stderr, "TransferErrorData[0] wrong size.\n" );
    }
    ClassAd * second = dynamic_cast<ClassAd *>(ads[1]);
    if(! (second != NULL && second->size() == 0)) {
        fprintf( stderr, "TransferErrorData[1] wrong size.\n" );
    }

    return 0;
}
