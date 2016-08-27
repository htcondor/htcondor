#include "daemon.h"
#include "dc_annexd.h"
#include "condor_attributes.h"

DCAnnexd::DCAnnexd( const char * name, const char * pool ) :
	Daemon( DT_GENERIC, name, pool ) { }

DCAnnexd::~DCAnnexd() { }

bool
DCAnnexd::sendBulkRequest( ClassAd const * request, ClassAd * reply, int timeout ) {
	setCmdStr( "sendBulkRequest" );

	ClassAd command( * request );
	command.Assign( ATTR_COMMAND, getCommandString( CA_BULK_REQUEST ) );
	command.Assign( "RequestVersion", 1 );

	return sendCACmd( & command, reply, true, timeout );
}
