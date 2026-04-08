#include "condor_common.h"

#include <optional>
#include <vector>
#include <string>
#include "classad/classad.h"

#include "classad_string_utils.h"


std::optional< std::vector<std::string> >
LookupClassAdListOfStrings( const classad::ClassAd & ad, const std::string & attribute ) {
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
AssignClassAdListOfStrings(
    classad::ClassAd & ad,
    const std::string & attribute,
    const std::ranges::view auto & list
) {
    classad::ExprList * el = new classad::ExprList();
    if( el == NULL ) { return false; }

    for( const auto & s : list ) {
        el->push_back( classad::Literal::MakeString(s) );
    }

    ad.Insert( attribute, el );
    return true;
}


// It makes me sad that `catalog_utils.h` needs `compat_classad.h`.
#include "compat_classad.h"
#include "catalog_utils.h"

// It seems like this shouldn't be necessary here, but an `auto` parameter
// in a function signature actually means `template`, so it wasn't required
// by the declaration of this function in the header.
#include <ranges>


// In this declaration, the reference inside `declval<>()` is critical, because
// the template _names_ in the type returned by  std::ranges::views::keys<T>()
// varies depending on whether the argument can be _promoted_ to reference!
using the_keys_type = decltype( std::ranges::views::keys(
    std::declval<ListOfCatalogs &>()
) );


template bool
AssignClassAdListOfStrings(
    classad::ClassAd & ad,
    const std::string & attribute,
    const the_keys_type & list
);
