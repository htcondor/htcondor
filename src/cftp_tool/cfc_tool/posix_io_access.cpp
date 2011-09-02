#include <cstdio>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#include <iostream>
#include "posix_io_access.h"

namespace cfc
{

    FileList PosixIOLayer::ListDirFiles(std::string dir_path)
    { 
        std::vector<std::string> dir_contents;
        std::vector<std::string> paths;
        std::string full_path;

        paths.push_back( dir_path );
        paths.push_back( "" );

        DIR *dp;
        struct dirent *ep;   
        struct stat buf;
  
        dp = opendir (dir_path.c_str());
        
        if (dp != NULL)
            {
                while (ep = readdir (dp))
                    {
                        paths[1] = std::string( ep->d_name );
                        full_path = JoinPaths( paths );
                        std::cerr << full_path << std::endl;


                        if(stat(full_path.c_str(), &buf)==-1)
                                continue;

                        // Push the file into list if it is regular
                        if( S_ISREG(buf.st_mode)!=0 )
                            {
                                dir_contents.push_back( full_path );
                            }
                    }
                (void) closedir (dp);
            }
        
        return dir_contents;
    }
    
    
    time_t PosixIOLayer::GetModifiedTime( std::string file_path )
    {
        
        struct stat buf;
        
        if( stat(file_path.c_str(), &buf) == -1 )
            {
                return -1;
            }
        else
            {
                return buf.st_mtime;
            }
        
    }
    
    
    
    bool PosixIOLayer::CopyFile( std::string source_path, std::string dest_path )
    {
        FILE* src;
        FILE* dest;
        char bit;
        
        src = dest = NULL;
        
        src = fopen( source_path.c_str(), "rb" );
        if( !src )
            return false;
        
        dest = fopen( dest_path.c_str(), "wb" );
        if( !dest )
            return false;
        
        
        bit = fgetc(src);
        while( !feof(src) )
            {
                fputc( bit, dest );
                bit = fgetc(src);
            }
        
        return true;
    }
    
    
    std::string PosixIOLayer::JoinPaths( std::vector<std::string> path_parts )
    {
        
        std::string final_path;
        std::string path_part;
        
        for( std::vector<std::string>::iterator iter = path_parts.begin();
             iter != path_parts.end();
             iter++ )
            {
                path_part = *iter;
                final_path += "/";
                final_path += path_part;
            }
        
        
        return final_path;
    }
    
    
    
}
