#ifndef _CFC_TOOL_DATABASE_H_
#define _CFC_TOOL_DATABASE_H_


#include <string>
#include <vector>
#include <db_cxx.h>

namespace cfc
{

    class CFCDatabase
    {
        
    public:
        CFCDatabase(std::string db_path);
        ~CFCDatabase();
        
        void insertFileLocation( const std::string &fileKey, const std::string &hostKey);
        void removeFileLocation( const std::string &fileKey, const std::string &hostKey);
        std::vector<std::string> getFileLocations( const std::string &fileKey );
        std::vector<std::string> getLocationFiles( const std::string &hostKey );
             
              
    private:
        
        std::string _db_path;

        void initDatabase();
        void closeDatabase();
        
        Db* DB_Files_to_Loc;
        Db* DB_Locs_to_File;

        DbEnv Environment;
        

    };
};

#endif
