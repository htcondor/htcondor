/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
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


#include "condor_common.h"
#include "analysis.h"

int numAttrs = 0;
const int NUM_OPS = 6;
string* attrs = NULL;
string operators[NUM_OPS] = { "<", "<=", "==", "!=", ">=", ">" };

void usage( char *myName );
bool generateAttributes( );
bool generateClassAdList( int numAds, int reqLen, int numDiffReqs,
						  ClassAdList &adList );
bool generateClassAd( int reqLen, classad::ClassAd *&ad );

int main ( int argc, char *argv[] )
{
//	PrettyPrint pp;
	int requestReqLen = 0;
	int offerReqLen = 0;
	int numOffers = 0;
	int numUniqueOfferReqs = 0;
	classad::ClassAd* request = NULL;
	classad::ClassAd* offer = NULL;
	ClassAdList offerList;
	ClassAdAnalyzer analyzer;
	string buffer = "";
//	int starttime = 0;
//	int endtime = 0;

	struct timeval *tvStart = new timeval;
	struct timeval *tvEnd = new timeval;
	struct timezone *tz = NULL;

	srand( time( NULL ) );

	if( argc != 6 && argc != 7 ) {
		usage( argv[0] );
		exit( 1 );
	}
	else {
		numAttrs = atoi( argv[2] );
		if( numAttrs <= 0 ) {
			cerr << argv[0] << ": bad value for number of attributes: "
				 << numAttrs << endl;
			exit( 1 );
		}
		if( !generateAttributes( ) ) {
			cerr << argv[0] << ": error in generateAttributes( )" << endl;
			exit( 1 );
		}

		requestReqLen = atoi( argv[3] );
		if( requestReqLen <= 0 ) {
			cerr << argv[0] << ": bad value for request requirements length: "
				 << requestReqLen << endl;
			exit( 1 );
		}
		if( !generateClassAd( requestReqLen, request ) ) {
			cerr << argv[0] << ": error in generateClassAd( " <<
					requestReqLen << ", request )" << endl;
			exit( 1 );
		}

		offerReqLen = atoi( argv[4] );
		if( offerReqLen <= 0 ) {
			cerr << argv[0] << ": bad value for offer requirements length: "
				 << offerReqLen << endl;
			exit( 1 );
		}

		if( strcmp( argv[1], "e" ) == 0 ) {
			if( argc != 6 ) {
				usage( argv[0] );
				exit( 1 );
			}
				// TEST AnalyzeExpressionToBuffer
			if( !generateClassAd( offerReqLen, offer ) ) {
				cerr << argv[0] << ": error in generateClassAd( " <<
					offerReqLen << ", offer )" << endl;
				exit( 1 );
			}
			string attr = argv[5];
//  			pp.Unparse( buffer, request );
//  			cout << "mainAd = " << buffer << endl;
//  			buffer = "";
//  			pp.Unparse( buffer, offer );
//  			cout << "contextAd = " << buffer << endl;
//  			buffer = "";
//  			cout << "attr = " << attr << endl;
//  			cout << "result:" << endl;
//  			cout << "-------" << endl;
			buffer = "";
			gettimeofday( tvStart, tz );
			analyzer.AnalyzeExprToBuffer( request, offer, attr, buffer );
			gettimeofday( tvEnd, tz );
			cout << ( round( ( tvEnd->tv_sec - tvStart->tv_sec ) * 100
							 + ( tvEnd->tv_usec - tvStart->tv_usec ) / 10000 )
					  / 100 ) << endl;
//  			cout << buffer;
			return 0;
		}
		else if( argc != 7 ) {
			usage( argv[0] );
			exit( 1 );
		}
		else {
			numOffers = atoi( argv[5] );
			if( numOffers <= 0 ) {
				cerr << argv[0] << ": bad value for number of offers: "
					 << numOffers << endl;
				exit( 1 );
			}
			numUniqueOfferReqs = atoi( argv[6] );
			if( numUniqueOfferReqs <= 0 || numUniqueOfferReqs > numOffers ) {
				cerr << argv[0] << ": bad value for number of unique offer requirements: "
					 << numUniqueOfferReqs << endl;
				exit( 1 );
			}
			if ( !generateClassAdList( numOffers, offerReqLen, 
									   numUniqueOfferReqs, offerList ) ) {
				cerr << argv[0] << ": error in generateClassAdList( "
					 << numOffers << ", " << offerReqLen << ", "
					 << numUniqueOfferReqs << ", offerList )" << endl;
				exit( 1 );
			}

			if( strcmp( argv[1], "jr" ) == 0 ) {
					// TEST AnalyzeJobReqToBuffer
//  				pp.Unparse( buffer, request );
//  				cout << "request = " << buffer << endl;
				buffer = "";
				gettimeofday( tvStart, tz );
				analyzer.AnalyzeJobReqToBuffer( toOldClassAd( request ), 
												offerList, buffer );
				gettimeofday( tvEnd, tz );
				cout << ( round( ( tvEnd->tv_sec - tvStart->tv_sec ) * 100
								 + ( tvEnd->tv_usec - tvStart->tv_usec )/10000)
						  / 100 ) << endl;
//  				cout << buffer;
				return 0;
			}
			else if( strcmp( argv[1], "ja" ) == 0 ) {
					// TEST AnalyzeJobAttrsToBuffer
//  				pp.Unparse( buffer, request );
//  				cout << "request = " << buffer << endl;
				buffer = "";
				gettimeofday( tvStart, tz );
				analyzer.AnalyzeJobAttrsToBuffer( toOldClassAd( request ), 
												  offerList, buffer );
				gettimeofday( tvEnd, tz );
				cout << ( round( ( tvEnd->tv_sec - tvStart->tv_sec ) * 100
								 + ( tvEnd->tv_usec - tvStart->tv_usec )/10000)
								 
						  / 100 ) << endl;
//  				cout << buffer;
				return 0;				
			}
			else {
				usage( argv[0] );
				exit( 1 );
			}
		}
	}
}

void 
usage( char *myName )
{
	printf( "Usage:\n" );
	printf( "%s jr|ja <#attrs> <request reqLen> <offer reqLen> <#offers> <#unique>\n", myName );
	printf( "%s e <#attrs> <main Ad reqLen> <context Ad reqLen> <attr>\n", myName);
}

bool
generateAttributes( )
{
	if( numAttrs <= 0 ) {
		return false;
	}
	attrs = new string[numAttrs];
	char tempAttr[32];
	for( int i = 0; i < numAttrs; i++ ){
		sprintf( tempAttr, "attr%i", i );
		attrs[i] = tempAttr;
	}
	return true;
}

bool
generateClassAdList( int numAds, int reqLen, int numDiffReqs,
						  ClassAdList &adList )
{
	classad::ClassAd *uniqueAds[numDiffReqs];
	classad::ClassAd *currentAd = NULL;
	for( int i = 0; i < numDiffReqs; i++ ) {
		uniqueAds[i] = NULL;
		if( !generateClassAd( reqLen, uniqueAds[i] ) ) {
			return false;
		}
	}
	adList.Rewind( );
	for( int i = 0; i < numAds; i++ ) {
		currentAd = (classad::ClassAd*)uniqueAds[rand( )%numDiffReqs]->Copy( );
		adList.Insert( toOldClassAd( currentAd ) );
	}
	adList.Rewind( );
	return true;
}

bool generateClassAd( int reqLen, classad::ClassAd *&ad )
{
	if( attrs == NULL || numAttrs <= 0) {
		return false;
	}
	classad::ClassAdParser parser;
	string adString = "";
	string reqString = "";
	char tempString[64];

	adString += '[';

	for( int i = 0; i < numAttrs; i++ ) {
		sprintf( tempString, "%s=%i;", attrs[i].c_str( ), rand( ) );
		adString += tempString;
	}

	for( int i = 0; i < reqLen; i++ ) {
		if( i > 0 ) {
			reqString += "&&";
		}
		sprintf( tempString, "(other.%s%s%i)",
				 attrs[rand( )%numAttrs].c_str(),
				 operators[rand( )%NUM_OPS].c_str( ),
				 rand( ) );
		reqString += tempString;
	}
	adString += ATTR_REQUIREMENTS;
	adString += '=';
	adString += reqString;

	adString += ']';
	if( !( ad = parser.ParseClassAd( adString ) ) ) {
		cerr << "ad = " << adString;
		return false;
	}
	else {
		return true;
	}
}

