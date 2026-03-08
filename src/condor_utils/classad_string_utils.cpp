#include "condor_common.h"

#include <optional>
#include <vector>
#include <string>
#include "classad/classad.h"

#include "classad_string_utils.h"

std::optional< std::vector<std::string> >
LookupClassAdStringList( const classad::ClassAd & ad, const std::string & attribute ) {
    classad::ExprTree * tree = ad.Lookup( attribute );
    if( tree == NULL ) { return {}; }

    classad::ExprList * list = dynamic_cast<classad::ExprList *>(tree);
    if( list == NULL ) { return {}; }

    std::vector< classad::ExprTree * > components;
    list->GetComponents(components);


    std::vector< std::string > rv;
    rv.reserve( components.size() );
    for( classad::ExprTree * e : components ) {
        classad::StringLiteral * sl = dynamic_cast<classad::StringLiteral *>(e);
        if( sl != NULL ) {
            rv.push_back( sl->getString() );
        }
    }

    return rv;
}


bool
AssignClassAdStringList(
    classad::ClassAd & ad,
    const std::string & attribute,
    const std::vector< std::string > & list
) {
    classad::ExprList * el = new classad::ExprList();
    if( el == NULL ) { return false; }

    for( const auto & s : list ) {
        el->push_back( classad::Literal::MakeString(s) );
    }

    ad.Insert( attribute, el );
    return true;
}
