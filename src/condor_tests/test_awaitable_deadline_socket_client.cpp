#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "condor_daemon_client.h"
#include "reli_sock.h"

int main( int argc, char ** argv ) {
    ASSERT( argc == 3 );

    config();

    std::string testID;
    if(0 == strcmp(argv[1], "-timeout")) {
        testID = "timeout";
    } else if(0 == strcmp(argv[1], "-hot")) {
        testID = "hot";
    } else {
        EXCEPT("First parameter must be -hot or -timeout.\n" );
    }

    std::string sinful = argv[2];


    Daemon * d = new Daemon(DT_ANY, sinful.c_str());
    ASSERT(d);
    Sock * r = d->startCommand(QUERY_STARTD_ADS, Stream::reli_sock, 0);
    ASSERT(r);


    r->encode();
    ASSERT(r->put(testID));
    ASSERT(r->end_of_message());


    std::string reply;

    r->decode();
    ASSERT(r->get(reply))
    ASSERT(r->end_of_message());


    if( starts_with(testID, "timeout") ) {
        ASSERT(sleep(25) == 0);
    }


    r->encode();
    ASSERT(r->put("the third string"));
    ASSERT(r->end_of_message());


    return 0;
}
