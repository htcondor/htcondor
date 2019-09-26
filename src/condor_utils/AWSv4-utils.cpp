#include "condor_common.h"
#include "condor_debug.h"

#include <string>
#include <map>
#include "classad/classad.h"
#include "AWSv4-utils.h"

//
// This function should not be called for anything in query_parameters,
// except for by AmazonQuery::SendRequest().
//
std::string
AWSv4Impl::amazonURLEncode( const std::string & input ) {
    /*
     * See http://docs.amazonwebservices.com/AWSEC2/2010-11-15/DeveloperGuide/using-query-api.html
     *
     *
     * Since the GAHP protocol is defined to be ASCII, we're going to ignore
     * UTF-8, and hope it goes away.
     *
     */
    std::string output;
    for( unsigned i = 0; i < input.length(); ++i ) {
        // "Do not URL encode ... A-Z, a-z, 0-9, hyphen ( - ),
        // underscore ( _ ), period ( . ), and tilde ( ~ ).  Percent
        // encode all other characters with %XY, where X and Y are hex
        // characters 0-9 and uppercase A-F.  Percent encode extended
        // UTF-8 characters in the form %XY%ZA..."
        if( ('A' <= input[i] && input[i] <= 'Z')
         || ('a' <= input[i] && input[i] <= 'z')
         || ('0' <= input[i] && input[i] <= '9')
         || input[i] == '-'
         || input[i] == '_'
         || input[i] == '.'
         || input[i] == '~' ) {
            char uglyHack[] = "X";
            uglyHack[0] = input[i];
            output.append( uglyHack );
        } else {
            char percentEncode[4];
            int written = snprintf( percentEncode, 4, "%%%.2hhX", input[i] );
            ASSERT( written == 3 );
            output.append( percentEncode );
        }
    }

    return output;
}

std::string
AWSv4Impl::canonicalizeQueryString(
    const std::map< std::string, std::string > & query_parameters ) {
    std::string canonicalQueryString;
    for( auto i = query_parameters.begin(); i != query_parameters.end(); ++i ) {
        // Step 1A: The map sorts the query parameters for us.  Strictly
        // speaking, we should encode into a different AttributeValueMap
        // and then compose the string out of that, in case amazonURLEncode()
        // changes the sort order, but we don't specify parameters like that.

        // Step 1B: Encode the parameter names and values.
        std::string name = amazonURLEncode( i->first );
        std::string value = amazonURLEncode( i->second );

        // Step 1C: Separate parameter names from values with '='.
        canonicalQueryString += name + '=' + value;

        // Step 1D: Separate name-value pairs with '&';
        canonicalQueryString += '&';
    }

    // We'll always have a superflous trailing ampersand.
    canonicalQueryString.erase( canonicalQueryString.end() - 1 );
    return canonicalQueryString;
}

