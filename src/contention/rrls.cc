#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "classad/classad.h"
#include <vector>

#include "daemon_list.h"
#include "condor_query.h"
#include "condor_claimid_parser.h"

//
// In order to compute the contention for a given resource, it is necessary
// to obtain the complete list of all resource request lists from each schedd
// in the pool.  The present negotiation protocol is submitter-oriented,
// where the negotiator asks each schedd for the RRLs associated with each
// submitter (that the schedd advertised).  Thus:
//
//      1.  Obtain the complete set of submitter ads from the collector.
//      2.  For each submitter (ad):
//          3.  Call Matchmaker::startNegotiate().
//          4.  Call ResourceRequestList::getRequest() until the schedd runs out.
//


bool the_callback( void * list, ClassAd * ad ) {
    ((std::vector<ClassAd> *)list)->push_back(* ad);
    return false;
}


std::optional< std::vector<ClassAd> >
obtain_submitter_list() {
    CondorError error_stack;
    std::vector<ClassAd> submitter_list;
    CondorQuery submitter_query(ANY_AD);
    submitter_query.requestPrivateAttrs();
    submitter_query.addANDConstraint("MyType == \"" SUBMITTER_ADTYPE "\"");
    CollectorList * collector_list = CollectorList::create();


    auto result = collector_list->query(
        submitter_query, the_callback, (void *)&submitter_list, & error_stack
    );
    delete collector_list;

    if( result != Q_OK ) {
        fprintf( stderr, "Couldn't fetch ads: %s\n",
            error_stack.code() ? error_stack.getFullText(false).c_str() : getStrQueryResult(result)
        );
    }

    return submitter_list;
}


ReliSock *
start_negotiating( const ClassAd & submitter_ad ) {
    std::string sessionID;
    std::string capability;
    if( submitter_ad.LookupString( ATTR_CAPABILITY, capability ) ) {
        ClaimIdParser cidp(capability.c_str());
        sessionID = cidp.secSessionId();
    }

    Daemon schedd( & submitter_ad, DT_SCHEDD, 0 );
    ReliSock * sock = schedd.reliSock(20);
    if(! sock) {
        fprintf( stderr, "Failed to connect to schedd\n" );
        return NULL;
    }

    bool result = schedd.startCommand(
        NEGOTIATE,
        sock,
        20,
        nullptr,
        nullptr,
        false,
        // FIXME: This is supposed to bypass ALLOW_NEGOTIATOR and doesn't.
        sessionID.c_str()
    );
    if(! result) {
        fprintf( stderr, "Failed to send NEGOTIATE command\n" );
        delete sock;
        return NULL;
    }


    ClassAd negotiateAd;

    std::string submitter;
    submitter_ad.LookupString( ATTR_NAME, submitter );
    submitter_ad.LookupString( "OriginalName", submitter );
    negotiateAd.InsertAttr( ATTR_OWNER, submitter );

    // ...
    negotiateAd.InsertAttr( ATTR_AUTO_CLUSTER_ATTRS, "CPUs" );

    std::string submitterTag;
    // ... error-handling ...
    submitter_ad.LookupString( ATTR_SUBMITTER_TAG, submitterTag );
    negotiateAd.InsertAttr( ATTR_SUBMITTER_TAG, submitterTag );

    // ...
    negotiateAd.InsertAttr( ATTR_NEGOTIATOR_NAME, "[contention]" );


    sock->encode();
    if(! putClassAd(sock, negotiateAd)) {
        fprintf( stderr, "Failed to put negotiate ClassAd\n" );
        delete sock;
        return NULL;
    }

    if(! sock->end_of_message()) {
        fprintf( stderr, "Failed to send end-of-message\n" );
        delete sock;
        return NULL;
    }


    return sock;
}


std::optional< std::vector<ClassAd> >
obtain_rrls() {
    auto submitter_list = obtain_submitter_list();
    std::vector<ClassAd> rrls;


    // Talk to one schedd at a time.
    std::sort(
        submitter_list->begin(), submitter_list->end(),
        []( const ClassAd & lhs, const ClassAd & rhs ) -> bool {
            std::string lha;
            lhs.LookupString(ATTR_SCHEDD_IP_ADDR, lha);
            std::string rha;
            rhs.LookupString(ATTR_SCHEDD_IP_ADDR, rha);
            if( lha <= rha ) { return true; }
            return false;
        }
    );

    ReliSock * socket = NULL;
    std::string current_schedd;
    for( const auto & submitter : * submitter_list ) {
        std::string submitter_schedd;
        submitter.LookupString( ATTR_SCHEDD_IP_ADDR, submitter_schedd );
        if( current_schedd != submitter_schedd) {
            current_schedd = submitter_schedd;
            socket = start_negotiating( submitter );
        }

        if( socket == NULL ) {
            fprintf( stderr, "start_negotiating() failed, skipping.\n" );
            continue;
        }

        // There should be a loop here?  How does the real negotiator
        // know how many queries to make?  SEND_RESOURCE_REQUEST_LIST?
        //
        // OK, it looks like SEND_JOB_INFO is the old style and
        // SEND_RESOURCE_REQUEST_LIST is the new style (and probably
        // what we actually want).  We never actually send the schedd
        // how many replies we're expecting, but we only ever read 200
        // at a time.  It's not clear how this works on the schedd side,
        // since it looks like we just read an arbitrary number of times
        // and then issue another SEND_RESOURCE_REQUEST_LIST command.  Maybe
        // the subsequent ones are ignored?
        //
        // ... somehting about m_send_end_negotiate()

        // ... error-handling ...
        socket->encode();
        socket->put(SEND_JOB_INFO);
        socket->end_of_message();

        int reply;
        socket->decode();
        socket->get(reply);

        if( reply == NO_MORE_JOBS ) {
            socket->end_of_message();
            continue;
        } else {
            ClassAd * request_ad = new ClassAd();
            getClassAd(socket, * request_ad);
            socket->end_of_message();

            rrls.push_back(* request_ad);
        }
    }


    for( const auto & rrl : rrls ) {
        fPrintAd( stdout, rrl );
    }
    return rrls;
}


int
main( int, char ** ) {
    // Silently required for a lot of HTCondor functions.
    config();

    std::ignore = obtain_rrls();

    return 0;
}
