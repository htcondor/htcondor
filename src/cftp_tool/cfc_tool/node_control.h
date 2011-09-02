#ifndef _CFC_TOOL_NODE_CONTROL_H_
#define _CFC_TOOL_NODE_CONTROL_H_

#include <string>
#include <chimera.h>
#include <pthread.h>

#include "filemanager.h"
#include "cfcdatabase.h"

#define TEST 11

#define CFC_PUBLISH 12
#define CFC_UNPUBLISH 13
#define CFC_LOCATE 14
#define CFC_LOCATE_ACK 15

void _ext_forward_callback( Key **key, Message **msg,     ChimeraHost **host );
void _ext_deliver_callback( Key *key,  Message *msg );
void _ext_update_callback(  Key *key,  ChimeraHost *host, int joined);

namespace cfc
{
    typedef struct 
    {
        char key[KEY_SIZE / BASE_B + 1]; // File Hash String
        char host[1024];                 // Host (File Location) Encoding
    } PublishMsg;
    typedef PublishMsg UnPublishMsg;
    typedef PublishMsg LocateMsg;


    class NodeControl
    {
        
        
    public:
        NodeControl(int port,
                    std::string cache_path,
                    std::string db_path);
        NodeControl(int port,
                    std::string cache_path,
                    std::string db_path,
                    std::string bs_host,
                    int bs_port);
        ~NodeControl();
        
        void init();
        
        friend void ::_ext_forward_callback( Key **key, Message **msg, ChimeraHost **host );
        friend void ::_ext_deliver_callback( Key *key,  Message *msg );
        friend void ::_ext_update_callback(  Key *key,  ChimeraHost *host, int joined);
        
        void run_forever();
        
        
        // Interactive access methods

        std::string locate( std::string keystr, int timeout );

        
    private:
        
        int _port;
        std::string _cache_path;
        std::string _db_path;
        char _key[200];

        NodeControl() {throw 1;};
        NodeControl(const NodeControl& ) {throw 1;};
        
        // Handlers

        void  _forward( Key **key, Message **msg, ChimeraHost **host);
        void  _deliver( Key *key,  Message *msg );
        void  _update( Key *key, ChimeraHost *host, int joined ); 
        

        void _publish( Key k );
        void _unpublish( Key k );
        void _locate( Key k );
        void _locate_ack(LocateMsg* , std::string );
        void _handle_publish(PublishMsg*);
        void _handle_unpublish(UnPublishMsg*);
        void _handle_locate(LocateMsg*);
        void _handle_locate_ack(LocateMsg*);

        //Filemanagement

        FileManager* _filemanager;
        KeySet _local_files;

        int _file_check_rate;
        time_t _last_update_time;

        void _update_local_files();
        


        //Database

        CFCDatabase* _database;


        //Chimera State members
        
        ChimeraHost* _bootstrap_host;
        ChimeraState* _node_net_state;
        
    };


};

extern cfc::NodeControl* NODECONTROL;

#endif
