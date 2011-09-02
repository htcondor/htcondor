#ifndef _CFC_TOOL_POSIX_IO_ACCESS_H_
#define _CFC_TOOL_POSIX_IO_ACCESS_H_

#include <string>
#include <vector>
#include "io_access.h"


namespace cfc
{
    class PosixIOLayer : public IOLayer
    {

    public:
        PosixIOLayer() {};
        virtual ~PosixIOLayer() {};
        
        virtual FileList ListDirFiles(std::string dir_path);
        virtual time_t GetModifiedTime( std::string file_path );
        virtual bool CopyFile( std::string source_path, std::string dest_path );
        virtual std::string JoinPaths( PathList path_parts );
    };
};

#endif
