/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "collection.h"

ClassAdCollectionInterface::
ClassAdCollectionInterface( )
{
	log_fp = NULL;
	amode = DONT_WANT_ACKS;
	currentXactionName = "";
}


ClassAdCollectionInterface::
~ClassAdCollectionInterface( )
{
	if( log_fp ) fclose( log_fp );
}


ClassAd *ClassAdCollectionInterface::
_CreateSubView( const ViewName &viewName, const ViewName &parentViewName, 
	const string &constraint, const string &rank, const string &partitionExprs )
{
    ClassAd 	*rec;
	string		buffer;

	buffer = "[ ViewName = \"";
	buffer +=  viewName;
	buffer += "\" ; ParentViewName = \"";
	buffer += parentViewName;
	buffer += "\" ; Requirements = ";
	buffer += constraint=="" ? "true" : constraint;
	buffer += " ; PartitionExprs = ";
	buffer += partitionExprs=="" ? "{}" : partitionExprs;
	buffer += " ; Rank = ";
	buffer += rank=="" ? "undefined" : rank;	
	buffer += " ] ]";

	if( !( rec = parser.ParseClassAd( buffer ) ) ) {
		return( NULL );
	}
	rec->InsertAttr( "OpType", ClassAdCollOp_CreateSubView );
	return( rec );
}


ClassAd *ClassAdCollectionInterface::
_CreatePartition( const ViewName &viewName, const ViewName &parentViewName,
    const string &constraint, const string &rank, const string &partitionExprs,
    ClassAd *rep )
{
    ClassAd 	*rec;
	string				buffer;

	buffer = "[ ViewName = \"";
	buffer +=  viewName;
	buffer += "\" ; ParentViewName = \"";
	buffer += parentViewName;
	buffer += "\" ; Requirements = ";
	buffer += constraint=="" ? "true" : constraint;
	buffer += " ; PartitionExprs = ";
	buffer += partitionExprs=="" ? "{}" : partitionExprs;
	buffer += " ; Rank = ";
	buffer += rank=="" ? "undefined" : rank;	
	buffer += " ] ]";

	if( !( rec = parser.ParseClassAd( buffer ) ) ) {
		return( NULL );
	}
	rec->InsertAttr( "OpType", ClassAdCollOp_CreatePartition );
	rec->InsertAttr( "Representative", rep );
	return( rec );
}


ClassAd *ClassAdCollectionInterface::
_DeleteView( const ViewName &viewName )
{
    ClassAd *rec;
    if( !( rec = new ClassAd( ) ) ) {
        CondorErrno = ERR_MEM_ALLOC_FAILED;
        CondorErrMsg = "";
        return( (ClassAd*) NULL );
    }
    if( !rec->InsertAttr( "OpType", ClassAdCollOp_DeleteView )   ||
            !rec->InsertAttr( "ViewName", viewName ) ) {
        CondorErrMsg += "; failed to make delete view record";
        delete rec;
        return( (ClassAd*) NULL );
    }
    return( rec );
}


ClassAd *ClassAdCollectionInterface::
_SetViewInfo( const ViewName &viewName, const string &constraint,
    const string &rank, const string &partitionExprs )
{
    ClassAd 	*rec=NULL;
	string		buffer;

	buffer = "[ ViewName = \"";
	buffer +=  viewName;
	buffer += "\" ; ViewInfo = [ Requirements = ";
	buffer += constraint=="" ? "true" : constraint;
	buffer += " ; PartitionExprs = ";
	buffer += partitionExprs=="" ? "{}" : partitionExprs;
	buffer += " ; Rank = ";
	buffer += rank=="" ? "undefined" : rank;	
	buffer += " ] ]";

    if( !( rec = parser.ParseClassAd( buffer ) ) ) {
        return( (ClassAd*) NULL );
    }
	rec->InsertAttr( "OpType", ClassAdCollOp_SetViewInfo );
	return( rec );
}


ClassAd *ClassAdCollectionInterface::
_AddClassAd( const string &xactionName, const string &key,ClassAd *ad )
{
    ClassAd *rec;
    if( !( rec = new ClassAd( ) ) ) {
        CondorErrno = ERR_MEM_ALLOC_FAILED;
        CondorErrMsg = "";
        return( (ClassAd*) NULL );
    }
    if( ( !xactionName.empty( ) && 
				!rec->InsertAttr( ATTR_XACTION_NAME, xactionName ) )||
            !rec->InsertAttr( "OpType", ClassAdCollOp_AddClassAd )  ||
            !rec->InsertAttr( "Key", key ) 							||
            !rec->Insert( "Ad", ad ) ) {
        CondorErrMsg += "; failed to make add classad " + key + " record";
        delete rec;
        return( (ClassAd*) NULL );
    }
    return( rec );
}

ClassAd *ClassAdCollectionInterface::
_UpdateClassAd( const string &xactionName, const string &key, ClassAd *ad )
{
    ClassAd *rec;
    if( !( rec = new ClassAd( ) ) ) {
        CondorErrno = ERR_MEM_ALLOC_FAILED;
        CondorErrMsg = "";
        return( (ClassAd*) NULL );
    }
    if( ( !xactionName.empty( ) && 
			!rec->InsertAttr( ATTR_XACTION_NAME, xactionName ) )	||
            !rec->InsertAttr( "OpType",ClassAdCollOp_UpdateClassAd )||
            !rec->InsertAttr( "Key", key )                 			||
            !rec->Insert( "Ad", ad ) ) {
        CondorErrMsg += "; failed to make update classad " + key + " record";
        delete rec;
        return( (ClassAd*) NULL );
    }
    return( rec );
}

ClassAd *ClassAdCollectionInterface::
_ModifyClassAd( const string &xactionName, const string &key, ClassAd *ad )
{
    ClassAd *rec;
    if( !( rec = new ClassAd( ) ) ) {
        CondorErrno = ERR_MEM_ALLOC_FAILED;
        CondorErrMsg = "";
        return( (ClassAd*) NULL );
    }
    if( ( !xactionName.empty( ) && 
			!rec->InsertAttr( ATTR_XACTION_NAME, xactionName ) )	||
            !rec->InsertAttr( "OpType",ClassAdCollOp_ModifyClassAd )||
            !rec->InsertAttr( "Key", key )                 			||
            !rec->Insert( "Ad", ad ) ) {
        CondorErrMsg += "; failed to make modify classad " + key + " record";
        delete rec;
        return( (ClassAd*) NULL );
    }
    return( rec );
}

ClassAd *ClassAdCollectionInterface::
_RemoveClassAd( const string &xactionName, const string &key )
{
    ClassAd *rec;

    if( !( rec = new ClassAd( ) ) ) {
        CondorErrno = ERR_MEM_ALLOC_FAILED;
        CondorErrMsg = "";
        return( (ClassAd*) NULL );
    }
    if( ( !xactionName.empty( ) && 
			!rec->InsertAttr( ATTR_XACTION_NAME, xactionName ) ) 	||
            !rec->InsertAttr( "OpType",ClassAdCollOp_RemoveClassAd )||
            !rec->InsertAttr( "Key", key ) ) {
        CondorErrMsg += "; failed to make delete classad " + key + " record";
        delete rec;
        return( (ClassAd*) NULL );
    }
    return( rec );
}

//----------------------- Mode and transaction management --------------------
bool ClassAdCollectionInterface::
SetAcknowledgementMode( AckMode a )
{
	if( currentXactionName != "" ) {
		CondorErrno = ERR_CANNOT_CHANGE_MODE;
		CondorErrMsg = "transaction active, cannot change ack mode";
		return( false );
	}
	amode = ( a == WANT_ACKS ) ? WANT_ACKS : DONT_WANT_ACKS;
	return( true );
}


bool ClassAdCollectionInterface::
SetCurrentTransaction( const string &xactionName )
{
	if( xactionName == "" || IsActiveTransaction( xactionName ) ) {
		currentXactionName = xactionName;
		return( true );
	}
	return( false );
}

void ClassAdCollectionInterface::
GetCurrentTransaction( string &xactionName )
{
	xactionName = currentXactionName;
}

//----------------------- Logging utility functions ---------------------------
ClassAd *ClassAdCollectionInterface::
ReadLogEntry( FILE *fp )
{
	string	line;
	int		ch;

		// accumulate until end-of-line/file in buffer
	for( ch=getc( fp ); ch != EOF && ch != '\n'; ch=getc( fp ) ) {
		line += char(ch);
	}
		// parse buffer
    ClassAd *log_rec;
    if( !( log_rec = parser.ParseClassAd( line ) ) ) {
			// FIXME? need more recovery code here??
		CondorErrMsg += "; could not parse log entry";
        if( log_rec ) delete log_rec;
        return( NULL );
    }

    return log_rec;
}


bool ClassAdCollectionInterface::
WriteLogEntry( FILE *fp, ClassAd *rec, bool synch )
{
		// not loggable --- no error
	if( !fp ) return( true );

	string buffer;
	unparser.Unparse( buffer, rec );
	if(fprintf(fp,"%s\n",buffer.c_str())<0 || (synch && fsync(fileno(fp))<0)) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "failed to log operation: " + buffer;
		return( false );
	}
	return( true );
}


bool ClassAdCollectionInterface::
ReadLogFile( )
{
    int     fd;
    char    buf[16];

        // open the file and wrap a source around it
    if( ( fd = open( logFileName.c_str( ), O_RDWR | O_CREAT, 0600 ) ) < 0 ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        sprintf( buf, "%d", errno );
        CondorErrMsg = "failed to open log " + logFileName + " errno=" +
            string(buf);
        return( false );
    }
    if( ( log_fp = fdopen( fd, "r+" ) ) == NULL ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        sprintf( buf, "%d", fd );
        CondorErrMsg = "failed to fdopen(" + string( buf ) + ") file ";
        sprintf( buf, "%d", errno );
        CondorErrMsg += logFileName + " errno=" + string( buf );
        close( fd );
        return( false );
    }

        // read log records and execute in (non-persistent, immediate) mode
    ClassAd *logRec;
    while( ( logRec = ReadLogEntry( log_fp ) ) != 0 ) {
        if( !OperateInRecoveryMode( logRec ) ) {
			CondorErrno = ERR_FATAL_ERROR;
            CondorErrMsg+="; FATAL ERROR: failed when recovering from log "
				"file "+logFileName;
            return( false );
        }
    }

    return( true );
}


bool ClassAdCollectionInterface::
TruncateLog( )
{
    string  tmpLogFileName;
    int     newLog_fd;
    FILE    *newLog_fp;
	string	logLine;
    char    buf[16];

    if( logFileName.empty( ) ) {
        CondorErrno = ERR_BAD_LOG_FILENAME;
        CondorErrMsg = "no filename provided for log file";
        return( false );
    }

        // first create a temporary file
    tmpLogFileName = logFileName + ".tmp";
    newLog_fd=open( tmpLogFileName.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600 );
    if( newLog_fd < 0 ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        CondorErrMsg = "when truncating log, failed to open " + tmpLogFileName
            + " errno=";
        sprintf( buf, "%d", errno );
        CondorErrMsg += string( buf );
        return( false );
    }
    if( ( newLog_fp = fdopen( newLog_fd, "r+" ) ) == NULL ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        sprintf( buf, "%d", newLog_fd );
        CondorErrMsg = "when truncating log, failed to fdopen(" + string( buf );
        sprintf( buf, "%d", errno );
        CondorErrMsg += ") file " + tmpLogFileName + " errno=" + string( buf );
        return( false );
    }
        // dump current state to file
    if( !LogState( newLog_fp ) ) {
        CondorErrMsg += "; did not truncate log";
        return( false );
    }

        // close the log files
    fclose( log_fp );
    fclose( newLog_fp );

        // move the new/tmp log file over the old log
#if defined(WIN32)
    if( MoveFileEx(tmpLogFileName.c_str( ), logFileName.c_str( ),
        MOVEFILE_REPLACE_EXISTING) == 0 ) {
        CondorErrno = ERR_RENAME_FAILED;
        sprintf( buf, "%d", GetLastError( ) );
        CondorErrMsg = "failed to truncate log: MoveFileEx failed with "
            "error=" + string( buf );
        return false;
    }
#else
    if( rename(tmpLogFileName.c_str( ), logFileName.c_str( ) ) < 0 ) {
        CondorErrno = ERR_RENAME_FAILED;
        sprintf( buf, "%d", errno );
        CondorErrMsg = "failed to truncate log: rename(" + tmpLogFileName +
            "," + logFileName + ") returned errno=" + string( buf );
        return( false );
    }
#endif

        // re-open new log file
    if( ( log_fp = fopen( logFileName.c_str( ), "a+" ) ) == NULL ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        sprintf( buf, "%d", errno );
        CondorErrMsg = "failed to reopen "+logFileName+", errno="+string(buf);
        return( false );
    }

    return( true );
}
