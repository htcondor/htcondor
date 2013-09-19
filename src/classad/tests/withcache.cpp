/***************************************************************
 *
 * Copyright (C) 1990-2012, Red Hat Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#define BOOST_TEST_MAIN
#define BOOST_AUTO_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <time.h>

#include "classad/classad.h"
#include "classad/classadCache.h"

using namespace std;
using namespace classad;


/////////////////////////////////////////////////////////////////
// State Space for tests: 
//
// 1. Insertion only 
// 2. Insertion and deleting 
// 3. Insert + sort + lookup.  
// 4. Insert + evaluate
// 5. Insertion + random deletion 
// 6. Insertion + queries. 

/////////////////////////////////////////////////////////////////
//BOOST_AUTO_TEST_SUITE( collector_auto )
#define BOOST_TEST_MODULE GRNN test suite

// --------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( example_test )
{
    ClassAdSetExpressionCaching(true); 
    
    vector< classad_shared_ptr<ClassAd> > ads;
    vector<string> inputData;
    classad_shared_ptr<ClassAd> pAd(new ClassAd);
    
    string szInput;
    ifstream infile;
    
    infile.open ("testdata.txt");
    if ( infile.fail() ) {
        cout << "Failed to open file" << endl;
        throw std::exception();
    }

#if 1
    clock_t Start = clock();

    while ( !infile.fail() && !infile.eof() )
    {
       getline( infile, szInput );

        // This is the end of an add.
        if (!szInput.length())
        {
            ads.push_back(pAd);
            pAd.reset( new ClassAd );
        }
        else if ( !pAd->Insert(szInput) )
        {
            cout<<"BARFED ON:"<<szInput<<endl;
            throw std::exception();
        }
    }
    if ( infile.fail() && !infile.eof() ) {
        cout << "File IO failure" << endl;
        throw std::exception();
    }
    
    cout<<"Clock Time:"<<(1.0*(clock() - Start))/CLOCKS_PER_SEC<<endl;

    infile.close();
#else
    while ( !infile.eof() )
    {
       getline( infile, szInput );
       inputData.push_back( szInput );
    }
    
    infile.close();
    
    clock_t Start = clock();
    
    for (unsigned int iCtr=0; iCtr<inputData.size(); iCtr++)
    {
        // This is the end of an add.
        if (!inputData[iCtr].length())
        {
            ads.push_back(pAd);
            pAd.reset( new ClassAd );
        }
        else if ( !pAd->Insert(inputData[iCtr]) )
        {
            cout<<"BARFED ON:"<<inputData[iCtr]<<endl;
            throw std::exception();
        }
    }
    
    cout<<"Clock Time:"<<(1.0*(clock() - Start))/CLOCKS_PER_SEC<<endl;
    inputData.clear();
#endif
    // enable this to look at the cache contents and debug data
    CachedExprEnvelope::_debug_dump_keys("output.txt");
    
    ads.clear();
    
    BOOST_CHECK_NO_THROW(); 
}

//BOOST_AUTO_TEST_SUITE_END()
//////////////////////////////////////////////////////////////////
