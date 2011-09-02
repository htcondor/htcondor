#include "cfcdatabase.h"
#include <string>
#include <vector>
#include <iostream>

int main(int argc, char** argv )
{

    cfc::CFCDatabase database( "/tmp" );

    // Make some test insertions

    database.insertFileLocation( "Cat.File", "192.168.0.1" );
    database.insertFileLocation( "Dog.File", "192.168.0.1" );
    database.insertFileLocation( "Bird.File", "192.168.0.1" );
    database.insertFileLocation( "Horse.File", "192.168.0.1" );
    database.insertFileLocation( "Duck.File", "192.168.0.1" );

    database.insertFileLocation( "Cat.File", "192.168.0.2" );
    database.insertFileLocation( "Bird.File", "192.168.0.2" );
    database.insertFileLocation( "Horse.File", "192.168.0.2" );

    database.insertFileLocation( "Cat.File", "192.168.0.3" );
    database.insertFileLocation( "Dog.File", "192.168.0.3" );
    database.insertFileLocation( "Horse.File", "192.168.0.3" );
    database.insertFileLocation( "Fish.File", "192.168.0.3" );

    // Now retrieve some stuff

    std::vector<std::string> results;
    typedef std::vector<std::string>::iterator res_iter;
    res_iter i;

    std::cout << "=======================================" << std::endl;

    results = database.getFileLocations( "Cat.File" );
    std::cout << "Locations for Cat.file: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }

    results = database.getFileLocations( "Dog.File" );
    std::cout << "Locations for Dog.file: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }

    results = database.getFileLocations( "Fish.File" );
    std::cout << "Locations for Fish.file: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }

    results = database.getFileLocations( "Snake.File" );
    std::cout << "Locations for Snake.file: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }   

    //- -----------------------------------------------


    results = database.getLocationFiles( "192.168.0.1" );
    std::cout << "Files at 192.168.0.1: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }

    results = database.getLocationFiles( "192.168.0.3" );
    std::cout << "Files at 192.168.0.3: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }

    results = database.getLocationFiles( "192.168.0.5" );
    std::cout << "Files at 192.168.0.5: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }  

    // Do some test Deletes

    std::cout << "=====================================" << std::endl;

    results = database.getFileLocations( "Cat.File" );
    std::cout << "Locations for Cat.file: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }

    std::cout << "Removing Cat.file from 192.168.0.1" << std::endl;

    database.removeFileLocation( "Cat.File", "192.168.0.1" );

    results = database.getFileLocations( "Cat.File" );
    std::cout << "Locations for Cat.file: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }

    std::cout << "Removing Cat.file from 192.168.0.12" << std::endl;

    database.removeFileLocation( "Cat.File", "192.168.0.12" );

    results = database.getFileLocations( "Cat.File" );
    std::cout << "Locations for Cat.file: " << std::endl;
    for( i = results.begin(); i != results.end(); i++)
        {
            std::cout << "\t" << *i << std::endl;
        }    


    return 0; 
}
