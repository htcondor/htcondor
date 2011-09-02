#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "chimera.h"
#include <pthread.h>




void test_fwd (Key ** kp, Message ** mp, ChimeraHost ** hp)
{

    Key *k = *kp;
    Message *m = *mp;
    ChimeraHost *h = *hp;

    pthread_t         self;

    self = pthread_self();
    fprintf( stderr, "----Called by thread (%ld)----\n", self );


    fprintf (stderr, "Routing %s (%s) to %s via %s:%d\n",
	     (m->type == TEST_CHAT) ? ("CHAT") : ("JOIN"), m->payload,
	     k->keystr, h->name, h->port);

    fprintf( stderr, "--------------------\n");

}

void test_del (Key * k, Message * m)
{

    pthread_t         self;
    self = pthread_self();
    fprintf( stderr, "----Called by thread (%ld)----\n", self );


    fprintf (stderr, "Delivered %s (%s) to %s\n",
	     (m->type == TEST_CHAT) ? ("CHAT") : ("JOIN"), m->payload,
	     k->keystr);

    if (m->type == TEST_CHAT)
	{
	    fprintf (stderr, "** %s **\n", m->payload);
	}

    fprintf( stderr, "--------------------\n");

}

void test_update (Key * k, ChimeraHost * h, int joined)
{

    pthread_t         self;
    self = pthread_self();
    fprintf( stderr, "----Called by thread (%ld)----\n", self);

    if (joined)
	{
	    fprintf (stderr, "Node %s:%s:%d joined neighbor set\n", k->keystr,
		     h->name, h->port);
	}
    else
	{
	    fprintf (stderr, "Node %s:%s:%d leaving neighbor set\n",
		     k->keystr, h->name, h->port);
	}

    fprintf( stderr, "--------------------\n");

}



int main( int argc, char** argv )
{
    ChimeraState* node_net_state;
    ChimeraHost*  bs_host;
    char* host;
    int port, bs_port;
    char tmp[256];
    Key key, keyinput;
    int i, j;
    pthread_t         self;

    bs_host = NULL;

    if( argc < 1 )
        return 1;
    else
        port = atoi( argv[1] );
    
    if( argc >= 4 )
        {
            host = argv[2];
            bs_port = atoi( argv[3] );
        }
    else
        host = NULL;    

    // Initilize the Chimera overlay library
    node_net_state = chimera_init( port );


    if (node_net_state == NULL)
	{
	    fprintf (stderr, "unable to initialize chimera \n");
	    exit (1);
	}
    if (host != NULL)
	{
	    bs_host = host_get (node_net_state, host, bs_port);
	}

    
    chimera_forward (node_net_state, test_fwd);
    chimera_deliver (node_net_state, test_del);
    chimera_update (node_net_state, test_update);

    chimera_join (node_net_state, bs_host);

    while (fgets (tmp, 256, stdin) != NULL)
	{
        self = pthread_self();
        fprintf( stderr, "----Loop running in by thread (%ld)----\n", self );

	    if (strlen (tmp) > 2)
		{
		    for (i = 0; tmp[i] != '\n'; i++);
		    tmp[i] = 0;
		    for (i = 0; tmp[i] != ' ' && i < strlen (tmp); i++);
		    tmp[i] = 0;
		    i++;
		    str_to_key (tmp, &key);
		    fprintf (stderr, "sending key:%s data:%s len:%d\n",
			     key.keystr, tmp + i, strlen (tmp + i));
		    chimera_send (node_net_state, key, TEST_CHAT, strlen (tmp + i) + 1,
				  tmp + i);
		}

        fprintf( stderr, "-----------------------" );
	}


}
