#include "condor_common.h"
#include "condor_sinful.h"

#include <stdio.h>

bool verbose = false;
#define REQUIRE( condition ) \
	if(! ( condition )) { \
		fprintf( stderr, "Failed requirement '%s' on line %d.\n", #condition, __LINE__ ); \
		return 1; \
	} else if( verbose ) { \
		fprintf( stdout, "Passed requirement '%s' on line %d.\n", #condition, __LINE__ ); \
	}

int main( int, char ** ) {
	Sinful s;
	Sinful t;

	//
	// Test, for 0, 1, 2, and 3 condor_sockaddrs: that the vector is not NULL,
	// that the vector has the correct number of elements, that the vector's
	// elements values are correct (and in the correct order); and that the
	// string form of the sinful is correct; and that the parser correctly
	// handles the sinful string (by repeating the vector tests); and that
	// the string->sinful->string loop also works (by repeating the string
	// test).  Then repeat the 0 test after clearing the Sinful of addrs.
	//
	// We then also test adding three condor_sockaddrs in a row, just in
	// case one-at-a-time testing somehow hides a problem; this also tests
	// that adding condor_sockaddrs after clearing the Sinful works.
	//


	const std::vector< condor_sockaddr > * v = &s.getAddrs();
	REQUIRE( v->size() == 0 );

	char const * sinfulString = NULL;
	sinfulString = s.getSinful();
	REQUIRE( sinfulString == NULL );

	REQUIRE( ! s.hasAddrs() );


	condor_sockaddr sa;
	bool ok = sa.from_ip_and_port_string( "1.2.3.4:5" );
	REQUIRE( ok );
	s.addAddrToAddrs( sa );

	v = &s.getAddrs();
	REQUIRE( v->size() == 1 );
	REQUIRE( (*v)[0] == sa );

	sinfulString = s.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5>" ) == 0 );

	t = Sinful( sinfulString );
	v = &t.getAddrs();
	REQUIRE( v->size() == 1 );
	REQUIRE( (*v)[0] == sa );

	sinfulString = t.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5>" ) == 0 );

	REQUIRE( s.hasAddrs() );


	condor_sockaddr sa2;
	ok = sa2.from_ip_and_port_string( "5.6.7.8:9" );
	REQUIRE( ok );
	s.addAddrToAddrs( sa2 );

	v = &s.getAddrs();
	REQUIRE( v->size() == 2 );
	REQUIRE( (*v)[0] == sa );
	REQUIRE( (*v)[1] == sa2 );

	sinfulString = s.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5+5.6.7.8-9>" ) == 0 );

	t = Sinful( sinfulString );
	v = &t.getAddrs();
	REQUIRE( v->size() == 2 );
	REQUIRE( (*v)[0] == sa );
	REQUIRE( (*v)[1] == sa2 );

	sinfulString = t.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5+5.6.7.8-9>" ) == 0 );

	REQUIRE( s.hasAddrs() );


	condor_sockaddr sa3;
	ok = sa3.from_ip_and_port_string( "[1:3:5:7::a]:13" );
	REQUIRE( ok );
	s.addAddrToAddrs( sa3 );

	v = &s.getAddrs();
	REQUIRE( v->size() == 3 );
	REQUIRE( (*v)[0] == sa );
	REQUIRE( (*v)[1] == sa2 );
	REQUIRE( (*v)[2] == sa3 );

	sinfulString = s.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5+5.6.7.8-9+[1-3-5-7--a]-13>" ) == 0 );

	t = Sinful( sinfulString );
	v = &t.getAddrs();
	REQUIRE( v->size() == 3 );
	REQUIRE( (*v)[0] == sa );
	REQUIRE( (*v)[1] == sa2 );
	REQUIRE( (*v)[2] == sa3 );

	sinfulString = t.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5+5.6.7.8-9+[1-3-5-7--a]-13>" ) == 0 );

	REQUIRE( s.hasAddrs() );


	s.clearAddrs();
	v = &s.getAddrs();
	REQUIRE( v->size() == 0 );

	sinfulString = s.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<>" ) == 0 );

	t = Sinful( sinfulString );
	v = &t.getAddrs();
	REQUIRE( v->size() == 0 );

	sinfulString = t.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<>" ) == 0 );

	REQUIRE( ! s.hasAddrs() );


	s.addAddrToAddrs( sa );
	s.addAddrToAddrs( sa2 );
	s.addAddrToAddrs( sa3 );

	v = &s.getAddrs();
	REQUIRE( v->size() == 3 );
	REQUIRE( (*v)[0] == sa );
	REQUIRE( (*v)[1] == sa2 );
	REQUIRE( (*v)[2] == sa3 );

	sinfulString = s.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5+5.6.7.8-9+[1-3-5-7--a]-13>" ) == 0 );

	t = Sinful( sinfulString );
	v = &t.getAddrs();
	REQUIRE( v->size() == 3 );
	REQUIRE( (*v)[0] == sa );
	REQUIRE( (*v)[1] == sa2 );
	REQUIRE( (*v)[2] == sa3 );

	sinfulString = t.getSinful();
	REQUIRE( sinfulString != NULL );
	REQUIRE( strcmp( sinfulString, "<?addrs=1.2.3.4-5+5.6.7.8-9+[1-3-5-7--a]-13>" ) == 0 );

	REQUIRE( s.hasAddrs() );


	// In practice, all C++03 implementations are stable with respect
	// to insertion order; the  C++11 standard requires that they are.
	// This is the unit test for the former.
	std::multimap< int, condor_sockaddr > sortedByDesire;

	sortedByDesire.insert(std::make_pair( 0, sa ));
	sortedByDesire.insert(std::make_pair( 0, sa2 ));
	sortedByDesire.insert(std::make_pair( 0, sa3 ));
	int i = 0;
	std::multimap< int, condor_sockaddr >::const_iterator iter;
	for( iter = sortedByDesire.begin(); iter != sortedByDesire.end(); ++iter, ++i ) {
		switch( i ) {
			case 0:
				REQUIRE( (* iter).second == sa );
				break;
			case 1:
				REQUIRE( (* iter).second == sa2 );
				break;
			case 2:
				REQUIRE( (* iter).second == sa3 );
				break;
			default:
				REQUIRE( false );
				break;
		}
	}

	sortedByDesire.clear();
	REQUIRE( sortedByDesire.size() == 0 );
	sortedByDesire.insert(std::make_pair( 1, sa ));
	sortedByDesire.insert(std::make_pair( 0, sa2 ));
	sortedByDesire.insert(std::make_pair( 0, sa3 ));
	i = 0;
	for( iter = sortedByDesire.begin(); iter != sortedByDesire.end(); ++iter, ++i ) {
		switch( i ) {
			case 0:
				REQUIRE( (* iter).second == sa2 );
				break;
			case 1:
				REQUIRE( (* iter).second == sa3 );
				break;
			case 2:
				REQUIRE( (* iter).second == sa );
				break;
			default:
				REQUIRE( false );
				break;
		}
	}

	sortedByDesire.clear();
	REQUIRE( sortedByDesire.size() == 0 );
	sortedByDesire.insert(std::make_pair( 0, sa2 ));
	sortedByDesire.insert(std::make_pair( 1, sa ));
	sortedByDesire.insert(std::make_pair( 0, sa3 ));
	i = 0;
	for( iter = sortedByDesire.begin(); iter != sortedByDesire.end(); ++iter, ++i ) {
		switch( i ) {
			case 0:
				REQUIRE( (* iter).second == sa2 );
				break;
			case 1:
				REQUIRE( (* iter).second == sa3 );
				break;
			case 2:
				REQUIRE( (* iter).second == sa );
				break;
			default:
				REQUIRE( false );
				break;
		}
	}

	return 0;
}
