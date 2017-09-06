#include "condor_common.h"
#include "condor_sinful.h"

#include <string>
#include <time.h>

typedef struct {
	std::string addr;
	bool shouldHaveIPv4;
	bool shouldHaveIPv6;
} gsspb_t;

extern void getSinfulStringProtocolBools( bool isIPv4, bool isIPv6,
	const char * sinfulString, bool & v4, bool & v6 );

int
main( int /* argc */, char ** /* argv */ ) {
	gsspb_t testData[] = {
		{ "<[2607:f388:107c:501:62eb:69ff:fe3e:bb05]:9555?CCBID=128.104.100.28:9555%3faddrs%3d[2607-f388-107c-501-7a2b-cbff-fe0a-ae0d]-9555+128.104.100.28-9555%26noUDP%26sock%3dcollector#1&PrivNet=cm.batlab.org&addrs=[2607-f388-107c-501-62eb-69ff-fe3e-bb05]-9555&noUDP&sock=108706_2ab2_5>",
			false, true },
		{ "<128.104.100.22:9555?addrs=128.104.100.22-9555&noUDP&sock=172298_ace3_4>",
			true, false },
		{ "<128.104.100.22:9555?addrs=128.104.100.22-9555+[2607-f388-107c-501-62eb-69ff-fe3e-bb05]-9555&noUDP&sock=172298_ace3_4>",
			true, true },
		{ "<[2607:f388:107c:501:62eb:69ff:fe3e:bb05]:9555?CCBID=128.104.100.28:9555%3faddrs%3d[2607-f388-107c-501-7a2b-cbff-fe0a-ae0d]-9555+128.104.100.28-9555%26noUDP%26sock%3dcollector#1&PrivNet=cm.batlab.org&addrs=[2607-f388-107c-501-62eb-69ff-fe3e-bb05]-9555+128.104.100.22-9555&noUDP&sock=108706_2ab2_5>",
			true, true }
	};

	unsigned failures = 0;
	for( unsigned i = 0; i < sizeof(testData)/sizeof(gsspb_t); ++i ) {
		bool isIPv4 = false;
		bool isIPv6 = false;
		gsspb_t test = testData[i];

		// Can we parse without shortcuts?
		getSinfulStringProtocolBools( false, false, test.addr.c_str(), isIPv4, isIPv6 );
		if( isIPv4 != test.shouldHaveIPv4 ) {
			++failures;
			fprintf( stderr, "IPv4: %s != %s (%s)\n",
				isIPv4 ? "true" : "false",
				test.shouldHaveIPv4 ? "true" : "false",
				test.addr.c_str()
			);
		}
		if( isIPv6 != test.shouldHaveIPv6 ) {
			++failures;
			fprintf( stderr, "IPv6: %s != %s (%s)\n",
				isIPv6 ? "true" : "false",
				test.shouldHaveIPv6 ? "true" : "false",
				test.addr.c_str()
			);
		}

		// Do the shortcuts work?
		isIPv4 = isIPv6 = false;
		getSinfulStringProtocolBools( true, true, test.addr.c_str(), isIPv4, isIPv6 );
		if(! ((test.shouldHaveIPv4 && isIPv4) || (test.shouldHaveIPv6 && isIPv6))) {
			++failures;
			fprintf( stderr, "(true, true) shortcut failure (%s)\n", test.addr.c_str() );
		}

		isIPv4 = isIPv6 = false;
		getSinfulStringProtocolBools( true, false, test.addr.c_str(), isIPv4, isIPv6 );
		if(! ((test.shouldHaveIPv4 && isIPv4) || (test.shouldHaveIPv6 && isIPv6))) {
			++failures;
			fprintf( stderr, "(true, false) shortcut failure (%s)\n", test.addr.c_str() );
		}

		isIPv4 = isIPv6 = false;
		getSinfulStringProtocolBools( false, true, test.addr.c_str(), isIPv4, isIPv6 );
		if(! ((test.shouldHaveIPv4 && isIPv4) || (test.shouldHaveIPv6 && isIPv6))) {
			++failures;
			fprintf( stderr, "(false, true) shortcut failure (%s)\n", test.addr.c_str() );
		}
	}

	if( failures == 0 ) {
		fprintf( stdout, "No failures detected.\n" );
	}
	return failures;
}
