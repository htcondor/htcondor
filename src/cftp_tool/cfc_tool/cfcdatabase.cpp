#include "cfcdatabase.h"

#include <iostream>
#include <cstdlib>

cfc::CFCDatabase::CFCDatabase( std::string db_path) :
    Environment( 0 ),
    _db_path( db_path )
{ 

    initDatabase();


}


cfc::CFCDatabase::~CFCDatabase()
{

    closeDatabase();

}


void cfc::CFCDatabase::initDatabase()
{

    u_int32_t trunc_count;

    try
        {

            Environment.set_error_stream(&std::cerr);
            Environment.set_errpfx( "CFC_NODE_DBE" );

            Environment.open( _db_path.c_str(),
                              DB_INIT_MPOOL | DB_USE_ENVIRON | DB_CREATE | DB_THREAD, 0);

        }
    catch(DbException &e)
        {
            std::cerr << "Failed to open the DB environment." << std::endl;
            exit(20);
        }
    catch(std::exception &e)
        {
            std::cerr << "Failed to open the DB environment." << std::endl;
            exit(21);
        } 

    std::cerr << "Successfully created database environment." << std::endl;


    try 
        {
            DB_Files_to_Loc = new Db( &Environment, 0 );

            // Open the database
            DB_Files_to_Loc->set_flags(DB_DUPSORT); // Support Record Duplicates
            DB_Files_to_Loc->open(NULL,                // Transaction pointer 
                                 "files.db",          // Database file name 
                                 NULL,                // Optional logical database name
                                 DB_BTREE,            // Database access method
                                 DB_CREATE | DB_THREAD,              // Open flags
                                 0);                  // File mode (using defaults)

            DB_Files_to_Loc->truncate( NULL, &trunc_count, 0 );

            std::cerr << "Pruned " << trunc_count << " old records from last run. " << std::endl;

            // DbException is not subclassed from std::exception, so
            // need to catch both of these.
        }
    catch(DbException &e)
        {
            Environment.err(e.get_errno(), "Error Creating Files Db.");
            exit(20);
        }
    catch(std::exception &e)
        {
            Environment.errx("Error! %s", e.what());
            exit(21);
        } 


    try 
        {
            DB_Locs_to_File = new Db( &Environment, 0 );

            // Open the database
            DB_Locs_to_File->set_flags(DB_DUPSORT); // Support Record Duplicates
            DB_Locs_to_File->open(NULL,                // Transaction pointer 
                                 "locations.db",      // Database file name 
                                 NULL,                // Optional logical database name
                                 DB_BTREE,            // Database access method
                                 DB_CREATE | DB_THREAD,              // Open flags
                                 0);                  // File mode (using defaults)
            
            DB_Locs_to_File->truncate( NULL, &trunc_count, 0 );

            std::cerr << "Pruned " << trunc_count << " old records from last run. " << std::endl;

            // DbException is not subclassed from std::exception, so
            // need to catch both of these.
        }
    catch(DbException &e)
        {
            Environment.err(e.get_errno(), "Error Creating Location Db.");
            exit(20);
        }
    catch(std::exception &e)
        {
            Environment.errx("Error! %s", e.what());
            exit(21);
        }
    
}


void cfc::CFCDatabase::closeDatabase()
{
    
    // Database open and access operations happen here.
    
    try
        {
            // Close the databases
            DB_Files_to_Loc->close(0);
            DB_Locs_to_File->close(0);
        } 
     catch(DbException &e)
        {
            Environment.err(e.get_errno(), "Error in closing Dbs.");
        }
    catch(std::exception &e)
        {
            Environment.errx("Error! %s", e.what());
        }
    
    delete DB_Files_to_Loc;
    delete DB_Locs_to_File;


}



void cfc::CFCDatabase::insertFileLocation( const std::string &fileKey, const std::string &hostKey)
{
    Dbc *db_cursor;
    int ret;
    
    Dbt file_key((void*)fileKey.c_str(), fileKey.length()+1);
    Dbt loc_key((void*)hostKey.c_str(),  hostKey.length()+1); 
    
    try
        {
            // Add to Loc DB
            DB_Files_to_Loc->cursor(NULL, &db_cursor, 0);
            ret = db_cursor->put(&loc_key, &file_key, DB_KEYFIRST);
            db_cursor->close();

        }
    catch(DbException &e)
        {
            DB_Files_to_Loc->err(e.get_errno(), "Error!");
        }
    catch(std::exception &e)
        {
            DB_Files_to_Loc->errx("Error! %s", e.what());
        }

    try
        {
            // Add to Files DB
            DB_Locs_to_File->cursor(NULL, &db_cursor, 0);
            ret = db_cursor->put(&file_key, &loc_key, DB_KEYFIRST);
            db_cursor->close();

        }
    catch(DbException &e)
        {
            DB_Locs_to_File->err(e.get_errno(), "Error!");
        }
    catch(std::exception &e)
        {
            DB_Locs_to_File->errx("Error! %s", e.what());
        }
    

}


void cfc::CFCDatabase::removeFileLocation( const std::string &fileKey, const std::string &hostKey)
{

    Dbc *db_cursor;
    int ret;
    
    Dbt file_key((void*)fileKey.c_str(), fileKey.length()+1);
    Dbt loc_key((void*)hostKey.c_str(),  hostKey.length()+1); 
    
    try
        {
            // Add to Loc DB
            DB_Files_to_Loc->cursor(NULL, &db_cursor, 0);
            ret = db_cursor->get(&loc_key, &file_key, DB_GET_BOTH);
            if( ret == 0 )
                db_cursor->del(0);
            db_cursor->close();

        }
    catch(DbException &e)
        {
            DB_Files_to_Loc->err(e.get_errno(), "Error!");
        }
    catch(std::exception &e)
        {
            DB_Files_to_Loc->errx("Error! %s", e.what());
        }

    try
        {
            // Add to Files DB
            DB_Locs_to_File->cursor(NULL, &db_cursor, 0);
            ret = db_cursor->get(&file_key, &loc_key, DB_GET_BOTH);
            if( ret == 0 )
                db_cursor->del(0);
            db_cursor->close();

        }
    catch(DbException &e)
        {
            DB_Locs_to_File->err(e.get_errno(), "Error!");
        }
    catch(std::exception &e)
        {
            DB_Locs_to_File->errx("Error! %s", e.what());
        }


}



std::vector<std::string> cfc::CFCDatabase::getFileLocations( const std::string &fileKey )
{

    std::vector<std::string> locations;

    Dbc *db_cursor;
    int ret;

    Dbt search_key( (void*)fileKey.c_str(), fileKey.length()+1);

    try
        {
            // database open omitted for clarity
            
            // Get a cursor
            DB_Locs_to_File->cursor(NULL, &db_cursor, 0);
            
            // Set up our DBTs
            Dbt data;
            
            // Position the cursor to the first record in the database whose
            // key and data begin with the correct strings.
            int ret = db_cursor->get(&search_key, &data, DB_SET);
            while (ret != DB_NOTFOUND)
                {
                    locations.push_back(std::string((char*)data.get_data()));
                    ret = db_cursor->get(&search_key, &data, DB_NEXT_DUP);
                }

            db_cursor->close();
        }
    catch(DbException &e)
        {
            DB_Locs_to_File->err(e.get_errno(), "Error!");
        }
    catch(std::exception &e) 
        {
            DB_Locs_to_File->errx("Error! %s", e.what());
        }   


    return locations;
}

std::vector<std::string> cfc::CFCDatabase::getLocationFiles( const std::string &hostKey )
{

    std::vector<std::string> files;

    Dbc *db_cursor;
    int ret;

    Dbt search_key( (void*)hostKey.c_str(), hostKey.length()+1);

    try
        {
            // database open omitted for clarity
            
            // Get a cursor
            DB_Files_to_Loc->cursor(NULL, &db_cursor, 0);
            
            // Set up our DBTs
            Dbt data;
            
            // Position the cursor to the first record in the database whose
            // key and data begin with the correct strings.
            int ret = db_cursor->get(&search_key, &data, DB_SET);
            while (ret != DB_NOTFOUND)
                {
                    files.push_back(std::string((char*)data.get_data()));
                    ret = db_cursor->get(&search_key, &data, DB_NEXT_DUP);
                }

            db_cursor->close();
        }
    catch(DbException &e)
        {
            DB_Files_to_Loc->err(e.get_errno(), "Error!");
        }
    catch(std::exception &e) 
        {
            DB_Files_to_Loc->errx("Error! %s", e.what());
        }   


    return files;

}
