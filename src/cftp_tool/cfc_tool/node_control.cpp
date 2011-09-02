#include "node_control.h"
#include "posix_io_access.h"

#include <iostream>
#include <algorithm>

#include <cstring>
#include <cstdlib>

// For sleep()
#include <unistd.h>

// For clocks
#include <ctime>

//extern NodeControl* NODECONTROL;

// External callback functions

void _ext_forward_callback( Key **key, Message **msg,     ChimeraHost **host )
{
    if( NODECONTROL )
        NODECONTROL->_forward( key, msg, host );
    else
        std::cerr << "ERROR: NODECONTROL is void. Cannot handle messages." << std::endl;
}

void _ext_deliver_callback( Key *key,  Message *msg )
{
    if( NODECONTROL )
        NODECONTROL->_deliver( key, msg );
    else
        std::cerr << "ERROR: NODECONTROL is void. Cannot handle messages." << std::endl;

}

void _ext_update_callback(  Key *key,  ChimeraHost *host, int joined)
{
    if( NODECONTROL )
        NODECONTROL->_update( key, host, joined );
    else
        std::cerr << "ERROR: NODECONTROL is void. Cannot handle messages." << std::endl;

}



// Node Control

cfc::NodeControl::NodeControl(int port, std::string cache_path, std::string db_path) :
    _port( port ),
    _cache_path( cache_path ),
    _db_path( db_path ),
    _bootstrap_host( NULL ),
    _filemanager( NULL ),
    _local_files(),
    _file_check_rate( 30 ),
    _database( NULL )
{
    _node_net_state = chimera_init( _port );
    _filemanager = new FileManager( _cache_path.c_str(), new PosixIOLayer() );
    _local_files.clear();
    _database = new CFCDatabase( _db_path );
}

cfc::NodeControl::NodeControl(int port, std::string cache_path,  std::string db_path,
                              std::string bs_host, int bs_port ) :
    _port( port ),
    _cache_path( cache_path ),
    _db_path( db_path ),
    _bootstrap_host( NULL ),
    _filemanager( NULL ),
    _local_files(),
    _file_check_rate( 30 ),
    _database( NULL )
{
    _node_net_state = chimera_init( _port );
    _filemanager = new FileManager( _cache_path.c_str(), new PosixIOLayer() );
    _local_files.clear();
    _database = new CFCDatabase( _db_path );


    char * host;
   
    host = new char[ bs_host.length()+1 ];
    strcpy( host, bs_host.c_str() );

    _bootstrap_host = host_get(_node_net_state, host, bs_port);
    
    delete [] host;
}


cfc::NodeControl::~NodeControl()
{
    if( _bootstrap_host )
        host_release(_node_net_state, _bootstrap_host );

    if( _filemanager )
        delete _filemanager;

    if( _database )
        delete _database;
}

void cfc::NodeControl::init()
{
    // Publish message types
    chimera_register(_node_net_state, CFC_PUBLISH, 1 );
    chimera_register(_node_net_state, CFC_UNPUBLISH, 1 );
    chimera_register(_node_net_state, CFC_LOCATE, 1 );
    chimera_register(_node_net_state, CFC_LOCATE_ACK, 1 );


    chimera_forward (_node_net_state, _ext_forward_callback);
    chimera_deliver (_node_net_state, _ext_deliver_callback);
    chimera_update (_node_net_state, _ext_update_callback); 

    chimera_join (_node_net_state, _bootstrap_host);

    //Save my key for easy access
    memset( _key, 0, 200 );
    strcpy( _key, get_key_string(&((ChimeraGlobal *)_node_net_state->chimera)->me->key) );

}

void cfc::NodeControl::run_forever()
{
    _last_update_time = time(NULL);

    while( 1 )
        {
            if( time(NULL) - _last_update_time > _file_check_rate )
                {
                    std::cerr << "Running File Cache Check..." << std::endl;
                    _update_local_files();
                    _last_update_time = time(NULL);
                }

            sleep( 1 );
        };

}

//--------------------------------
// FileSystem Methods
//--------------------------------

void cfc::NodeControl::_update_local_files()
{
    KeySet raw_files;

    KeySet unchanged_files;
    KeySet new_files;
    KeySet missing_files;

    raw_files = _filemanager->GetKeys(true);


    // Run set operations to find unchanged, new and missing files
    // since the last time we ran
    keysetDiff( _local_files, raw_files, 
                unchanged_files, missing_files, new_files ); 

    std::cerr << "Current local files: " << std::endl;
    for( KeyIter l = _local_files.begin(); l != _local_files.end(); l++)
        {
            std::cerr << l->keystr << std::endl;
        }   

    std::cerr << "Reported files: " << std::endl;
    for( KeyIter r = raw_files.begin(); r != raw_files.end(); r++)
        {
            std::cerr << r->keystr << std::endl;
        }   

    
    // Publish new files.
    std::cerr << "New files found: " << std::endl;
    for( KeyIter n = new_files.begin(); n != new_files.end(); n++)
        {
            std::cerr << n->keystr << std::endl;

            _publish( *n );

        }


    // UnPublish missing files.
    std::cerr << "Missing files found: " << std::endl;
    for( KeyIter m = missing_files.begin(); m != missing_files.end(); m++)
        {
            std::cerr << m->keystr << std::endl;

            _unpublish( *m );


        }


    // Our local files should now be what the filemanager says to be true
    _local_files = raw_files;

}


//--------------------------------
// Interactive Methods
//--------------------------------

std::string cfc::NodeControl::locate( std::string keystr, int timeout )
{
    std::vector<std::string> locations = _database->getFileLocations( keystr );
    Key k;
    time_t tick;

    if( !locations.empty() )
        {
            std::random_shuffle( locations.begin(), locations.end() );
            return locations[0];
        }

    str_to_key( keystr.c_str(), &k);

    _locate( k );

    tick = time(NULL);
    while( time(NULL) - tick < timeout )
        {
            sleep(1);

            locations = _database->getFileLocations( keystr );
            if( !locations.empty() )
                {
                    std::random_shuffle( locations.begin(), locations.end() );
                    return locations[0];
                }
        }
    
    return "Key not found.";
}





//--------------------------------
// Node Control Top Level Handlers
//--------------------------------

void  cfc::NodeControl::_forward( Key **key, Message **msg, ChimeraHost **host )
{
    Key *k = *key;
    Message *m = *msg;
    ChimeraHost *h = *host;

    std::cerr << "FORWARD ( "<<_port<<" ) CALLED." << std::endl;

    if (m->type == CFC_PUBLISH)
	{
        std::cerr << "Publish message forwarded. Recording file location." << std::endl;
        std::cerr << m->payload << std::endl;
        _handle_publish((PublishMsg*)m->payload);
	}

    if (m->type == CFC_UNPUBLISH)
	{
        std::cerr << "UnPublish message forwarded. Forgetting file location." << std::endl;
        std::cerr << m->payload << std::endl;
        _handle_unpublish((UnPublishMsg*)m->payload);
	}

    if (m->type == CFC_LOCATE)
	{
        LocateMsg* req = (LocateMsg*)m->payload;
        std::vector<std::string> known_locations = _database->getFileLocations( req->key );

        if( ! known_locations.empty() )
            {
                // We know where this file can be found. Stop forwarding it.
                // set the destination to self
                str_to_key(_key, &(m->dest)); 
                // and set next hop to self
                host[0] = ((ChimeraGlobal *)_node_net_state->chimera)->me; 
            }
	}




    std::cerr << "Routing "
              << " (" << std::string(m->payload)
              << ") to " << k->keystr
              << " via " << h->name << ":" << h->port << std::endl;
    


    std::cerr << "FORWARD ( "<<_port<<" ) DONE." << std::endl;
}

void  cfc::NodeControl::_deliver(Key *key,  Message *msg )
{
    
    std::cerr << "DELIVER ( "<<_port<<" ) CALLED." << std::endl;

    std::cerr << "Delivered " 
              << " (" <<  std::string(msg->payload)
              <<") to " << key->keystr << std::endl;

    if (msg->type == CFC_PUBLISH)
	{
        std::cerr << "Publish message received. Recording file location." << std::endl;
        _handle_publish((PublishMsg*)msg->payload);
	}

    if (msg->type == CFC_UNPUBLISH)
	{
        std::cerr << "UnPublish message received. Forgetting file location." << std::endl;
        _handle_unpublish((UnPublishMsg*)msg->payload);
	}

    if (msg->type == CFC_LOCATE)
	{
        std::cerr << "Locate message received." << std::endl;
        _handle_locate((LocateMsg*)msg->payload);
	}

    if (msg->type == CFC_LOCATE_ACK)
	{
        std::cerr << "Locate message received." << std::endl;
        _handle_locate_ack((LocateMsg*)msg->payload);
	}
    

    std::cerr << "DELIVER ( "<<_port<<" ) DONE." << std::endl;

}

void  cfc::NodeControl::_update(Key *key, ChimeraHost *host, int joined  )
{

    std::cerr << "UPDATE ( "<<_port<<" ) CALLED." << std::endl;

    if (joined)
	{
	    std::cerr << "Node "<< key->keystr
                  <<":"<<host->name
                  <<":"<<host->port
                  <<" joined neighbor set"<<std::endl;
	}
    else
	{
	    std::cerr << "Node "<< key->keystr
                  <<":"<<host->name
                  <<":"<<host->port
                  <<" leaving neighbor set"<<std::endl;
	}

    
    std::cerr << "UPDATE ( "<<_port<<" ) DONE." << std::endl;

}



//------------------------------------------
// File Publishing Handlers
//------------------------------------------

void cfc::NodeControl::_publish(Key k )
{
    PublishMsg msg_data;
    ChimeraGlobal* g;

    g = (ChimeraGlobal*)(_node_net_state->chimera);

    memset( &msg_data, 0, sizeof( PublishMsg ) );
    strcpy(  msg_data.key, get_key_string( &k ) );
    host_encode( msg_data.host,
                 sizeof(msg_data.host), 
                 g->me );

    chimera_send(_node_net_state, 
                 k, CFC_PUBLISH, sizeof( PublishMsg ), (char*) &msg_data);

}

void cfc::NodeControl::_unpublish(Key k )
{
    UnPublishMsg msg_data;
    ChimeraGlobal* g;

    g = (ChimeraGlobal*)(_node_net_state->chimera);

    memset( &msg_data, 0, sizeof( UnPublishMsg ) );
    strcpy(  msg_data.key, get_key_string( &k ) );
    host_encode( msg_data.host,
                 sizeof(msg_data.host), 
                 g->me );

    chimera_send(_node_net_state,
                 k, CFC_UNPUBLISH, sizeof( UnPublishMsg ), (char*) &msg_data);
    
}

void cfc::NodeControl::_locate(Key k )
{
    LocateMsg msg_data;
    ChimeraGlobal* g;

    g = (ChimeraGlobal*)(_node_net_state->chimera);

    memset( &msg_data, 0, sizeof( UnPublishMsg ) );
    strcpy(  msg_data.key, get_key_string( &k ) );
    host_encode( msg_data.host,
                 sizeof(msg_data.host), 
                 g->me );

    chimera_send(_node_net_state,
                 k, CFC_LOCATE, sizeof( LocateMsg ), (char*) &msg_data);   
    
}

void cfc::NodeControl::_locate_ack(LocateMsg* msg, std::string host )
{
    LocateMsg msg_data;
    ChimeraHost* h;
    ChimeraGlobal* g;

    g = (ChimeraGlobal*)(_node_net_state->chimera);
    h = host_decode(_node_net_state, msg->host );

    memset( &msg_data, 0, sizeof( UnPublishMsg ) );
    strcpy(  msg_data.key, msg->key );
    strcpy(  msg_data.host, host.c_str() );

    chimera_send(_node_net_state,
                 h->key, CFC_LOCATE_ACK, sizeof( LocateMsg ), (char*) &msg_data);    

    host_release(_node_net_state, h );
    
}

void cfc::NodeControl::_handle_publish(PublishMsg* msg)
{
    std::string key  = msg->key;
    std::string host = msg->host;

    std::cerr << "Recording File " << key << " at location " << host << std::endl;


    _database->insertFileLocation( key, host );   
}

void cfc::NodeControl::_handle_unpublish(UnPublishMsg* msg)
{

    std::string key  = msg->key;
    std::string host = msg->host;

    std::cerr << "UnRecording File " << key << " at location " << host << std::endl;


    _database->removeFileLocation( key, host ); 
}

void cfc::NodeControl::_handle_locate(LocateMsg* msg)
{
    std::vector<std::string> known_locations = _database->getFileLocations( msg->key );

    // There should be a smart algorthim to find the best location
    // Perhaps by chosing a key closest to the requester.
    // For now we are simply picking a random key.
    random_shuffle ( known_locations.begin(), known_locations.end() );

    std::string location = known_locations[0];

    _locate_ack( msg, location );
}

void cfc::NodeControl::_handle_locate_ack(LocateMsg* msg )
{
    std::string key  = msg->key;
    std::string host = msg->host;

    std::cerr << "Recording File " << key << " at location " << host << std::endl;


    _database->insertFileLocation( key, host );
}
