#include "classad_package.h"

	// prototypes
void doMatchNotification( ClassAd *, ClassAd * );
void populateCollection( ClassAdCollection & );

	// typedefs; each resource and customer has a unique ID
typedef char ID[32];

int main( void )
{
		// resource and customer information
	ClassAdCollection	resources, 			customers;
	CollContentIterator	resourceItor, 		customerItor;
	ClassAd				*resourceClassAd, 	*customerClassAd;

		// the match environment to test for matches
	MatchClassAd		matchEnvironment;
	bool				match;

		// populate the collections with resources and customers
	printf( "Enter resource classads...\n" );
	populateCollection( resources );
	printf( "Enter customer classads...\n" );
	populateCollection( customers );


		// for each customer under consideration ...
	customers.InitializeIterator( 0 , &customerItor );
	while( !customerItor.AtEnd( ) ) {
			// ... plug the customer ad into the match environment, and ...
		customerItor.CurrentAd( customerClassAd );
		matchEnvironment.ReplaceLeftAd( customerClassAd );

			// ... go through the resources looking for a match
		resources.InitializeIterator( 0 , &resourceItor );
		while( !resourceItor.AtEnd( ) ) {
				// plug the resource into the match environment
			resourceItor.CurrentAd( resourceClassAd );
			matchEnvironment.ReplaceRightAd( resourceClassAd );

				// test for the match
			if( matchEnvironment.EvaluateAttrBool( "symmetricmatch", match ) &&
				match ) {
					// the classads matched each other; do match notification
				doMatchNotification( resourceClassAd, customerClassAd );
			}

				// next resource please ...
			matchEnvironment.RemoveRightAd( );
			resourceItor.NextAd( );
		}

			// next customer please ...
		matchEnvironment.RemoveLeftAd( );
		customerItor.NextAd( );
	}

	return( 0 );
}


void
populateCollection( ClassAdCollection &collection )
{
	Source	source;
	char	buffer[10240];
	ClassAd	*ad;
	ID		id;
	int		len;

		// until the user is done entering classads ...
	printf( "> " );
	fflush( stdout );
	fgets( buffer, 10240, stdin );
	while( strncmp( buffer, "done", 4 ) != 0 ) {

			// parse the buffer to yeild the classad
		source.SetSource( buffer );
		ad = NULL;
		if( !source.ParseClassAd( ad ) ) {
			cerr << "Parse error. Again." << endl;
		} else if( !ad->EvaluateAttrString( "ID", id, 32, len ) ) {
			cerr << "ClassAd did not have an 'ID' attribute.  Again." << endl;
			delete ad;
		} else {
				// insert the classad into the collection with the ID
			collection.NewClassAd( id, ad );
			cout << "Inserted: " << id << " -- " << buffer << endl;
		}

		printf( "> " );
		fflush( stdout );
		fgets( buffer, 10240, stdin );
	}
}


void
doMatchNotification( ClassAd *resource, ClassAd *customer )
{
	Sink			sink;
	FormatOptions	fo;

	cout << "\n\n ----- Found a match -----" << endl;
	fo.SetClassAdIndentation( true );
	sink.SetFormatOptions( &fo );
	sink.SetSink( stdout );

	cout << "Resource Ad:"; 
	resource->ToSink( sink );
	sink.FlushSink( );

	cout << "\nCustomer Ad:";
	customer->ToSink( sink );
	sink.FlushSink( );

	cout << endl << endl;
}
