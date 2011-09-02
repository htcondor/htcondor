#ifndef _CFC_TOOL_FILE_MANAGER_H_
#define _CFC_TOOL_FILE_MANAGER_H_

#include <ctime>

#include "keys.h"
#include "io_access.h"


namespace cfc
{
    class FileManager
    {
    public:
        FileManager(const char* storage_path, IOLayer* iol);
        virtual ~FileManager();

        bool InsertFile( const char* file_location, Key* key);
        bool FiletoKey( const char* file_location, Key* key);

        bool RetrieveFile( Key* key, const char* file_location);
        
        KeySet GetKeys(bool force_update=false);
        bool KeyExists( Key* key, bool force_update=false );

    private:

        int _update_rate;
        char* _storage_path;
        IOLayer* _iol;
        KeySet _knownkeys;
        time_t last_update_time;
        

        void update();

    };
};
#endif
