#include "filemanager.h"
#include "keys.h"
#include "posix_io_access.h"

int main( int argc, char** argv )
{
    cfc::FileManager* FM ;
    cfc::KeySet keys;

    key_init();

    FM = new cfc::FileManager( "./", new cfc::PosixIOLayer() );

    keys = FM->GetKeys();
    for( cfc::KeyIter iter = keys.begin();
         iter != keys.end();
         iter++)
        {
            key_print( *iter );
        }


    delete FM;

    return 0;
}
