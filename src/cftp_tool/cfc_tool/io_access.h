#ifndef _CFC_TOOL_IO_ACCESS_H_
#define _CFC_TOOL_IO_ACCESS_H_

#include <string>
#include <vector>
#include <time.h>

namespace cfc
{
    typedef std::vector<std::string> FileList;
    typedef std::vector<std::string>::iterator FileIter;

    typedef std::vector<std::string> PathList;
    typedef std::vector<std::string>::iterator PathIter;

    class IOLayer
    {
    public:
        IOLayer() {};
        virtual ~IOLayer() {};
        
        virtual FileList ListDirFiles(std::string dir_path) = 0;
        virtual time_t GetModifiedTime(std::string file_path ) = 0;
        virtual bool CopyFile( std::string source_path, std::string dest_path ) = 0;
        virtual std::string JoinPaths(PathList path_parts ) = 0;

    };
};

#endif
