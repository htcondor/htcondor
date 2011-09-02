#include "node_control.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>

cfc::NodeControl* NODECONTROL = NULL;

int main( int argc, char** argv )
{
    int port;
    std::string cache_path;
    std::string db_path;

    std::string boot_host;
    int boot_port = -1;

    if( argc < 3 )
        return 1;
    else
        {
            port = atoi( argv[1] );
            cache_path = std::string( argv[2] );
            db_path = std::string( argv[3] );
        }
    
    if( argc > 4 )
        {
            boot_host = std::string(argv[4]);
            boot_port = atoi( argv[5] );
        }   

    if( boot_port != -1 )
        {
            std::cerr << "Starting node in network: "<<
                boot_host <<":"<< boot_port << std::endl;
            std::cerr << "\tCache Path: " << cache_path << std::endl;
            std::cerr << "\tDatabase Path: " << db_path << std::endl;

            NODECONTROL = new cfc::NodeControl( port, cache_path, db_path,
                                                boot_host, boot_port );
        }
    else
        {
            std::cerr << "Starting new node network: "<< port<<std::endl;
            std::cerr << "\tCache Path: " << cache_path << std::endl;
            std::cerr << "\tDatabase Path: " << db_path << std::endl;

            NODECONTROL = new cfc::NodeControl( port, cache_path, db_path );
        }

    NODECONTROL->init();

    NODECONTROL->run_forever();

    
    delete NODECONTROL;
    NODECONTROL = NULL;
}
