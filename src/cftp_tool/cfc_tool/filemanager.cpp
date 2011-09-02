#include "filemanager.h"
#include "io_access.h"
#include <cstring>
#include <string.h>
#include <ctime>
#include <cstdlib>
#include <cstdio>

namespace cfc
{

FileManager::FileManager(const char* storage_path, IOLayer* iol)
{
    _storage_path = new char[strlen(storage_path)+1];
    strcpy( _storage_path, storage_path );

    _iol = iol;
    last_update_time = 0;

    update();
}


FileManager::~FileManager()
{

    if( _storage_path )
        delete [] _storage_path;

    if( _iol )
        delete _iol;

}

void FileManager::update()
{
    FileList files;
    FileIter iter;
    files = _iol->ListDirFiles( _storage_path);
    time_t file_m_time;
    Key temp_key;

    _knownkeys.clear();

    for( iter = files.begin(); iter != files.end(); iter++)
        {
            FiletoKey( iter->c_str(), &temp_key );
            _knownkeys.push_back( temp_key );
        }   
    
    last_update_time = time(NULL);
}

bool FileManager::InsertFile( const char* file_location, Key* key)
{

    if( !FiletoKey( file_location, key) )
        return false;

    _knownkeys.push_back(*key);

    PathList paths;
    paths.push_back( std::string( _storage_path ) );
    paths.push_back( std::string( key->keystr   ) );

    _iol->CopyFile( file_location, _iol->JoinPaths(paths) );
    return true;
}

bool FileManager::FiletoKey( const char* file_location, Key* key)
{

    FILE* f = NULL;
   
    f = fopen( file_location, "rb" );
    if( !f )
        return false;
    file_make_hash( key, f);
    fclose( f );
    return true;
}

bool FileManager::RetrieveFile( Key* key, const char* file_location)
{
    PathList paths;
    paths.push_back( std::string( _storage_path ) );
    paths.push_back( std::string( key->keystr   ) );

    return _iol->CopyFile( _iol->JoinPaths(paths), file_location );
}

KeySet FileManager::GetKeys(bool force_update)
{
    if( force_update == true || time(NULL) - last_update_time > _update_rate )
        update();

    return _knownkeys;
}

bool FileManager::KeyExists( Key* key, bool force_update)
{
    Key temp;

    if( force_update == true || time(NULL) - last_update_time > _update_rate )
        update();

    for( KeySet::iterator iter = _knownkeys.begin();
         iter != _knownkeys.end();
         iter ++ )
        {
            temp = *iter;

            if( key_equal( temp, *key )  == 1 )
                return true;
        }
    return false;
}

}
