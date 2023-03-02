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


#include "classad/common.h"
#include "classad/collection.h"
#include <fcntl.h>

using std::string;
using std::vector;
using std::pair;


namespace classad {

static bool string_is_empty(const string &text);
 
const char *const ClassAdCollectionInterface::CollOpStrings[] = {
    "no op",

    "create sub view",
    "create partition",
    "delete view",
    "set view info",
    "ack view operation",

    "add classad",
    "update classad",
    "modify classad",
    "remove classad",
    "ack classad op",

    "open transaction",
    "ack open transaction",
    "commit transaction",
    "abort transaction",
    "ack commit transaction",
    "forget transaction",

    "get classad",
    "get view info",
    "get sub view names",
    "get partitioned view names",
    "find partition name",
    "is active transaction?",
    "is committed transaction?",
    "get all active transactions",
    "get all committed transactions",
    "get server transaction state",
    "ack read op",

    "connect to server",
    "query view",
	"disconnect from server"
};


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
	if (string_is_empty(partitionExprs)) {
		buffer += "{}";
	} else {
		buffer += partitionExprs;
	}
	buffer += " ; Rank = ";
	if (string_is_empty(rank)) {
		buffer += "undefined";
	} else {
		buffer += rank;	
	}
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
	if (string_is_empty(partitionExprs)) {
		buffer += "{}";
	} else {
		buffer += partitionExprs;
	}
	buffer += " ; Rank = ";
	if (string_is_empty(rank)) {
		buffer += "undefied";
	} else {
		buffer += rank;
	}
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
	if (string_is_empty(partitionExprs)) {
		buffer += "{}";
	} else {
		buffer += partitionExprs;
	}
	buffer += " ; Rank = ";
	if (string_is_empty(rank)) {
		buffer += "undefined";
	} else {
		buffer += rank;
	}
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
    ExprTree * pAd = ad;
    if( !( rec = new ClassAd( ) ) ) {
        CondorErrno = ERR_MEM_ALLOC_FAILED;
        CondorErrMsg = "";
        return( (ClassAd*) NULL );
    }
    if( ( !xactionName.empty( ) && 
			!rec->InsertAttr( ATTR_XACTION_NAME, xactionName ) )||
            !rec->InsertAttr( "OpType", ClassAdCollOp_AddClassAd )  ||
            !rec->InsertAttr( "Key", key ) 							||
            !rec->Insert( "Ad", pAd ) ) {
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
GetCurrentTransaction( string &xactionName ) const
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // the seek, read, open, close, fileno, etc are deprecated, use _seek, etc instead.
#endif

bool ClassAdCollectionInterface::
WriteLogEntry( FILE *fp, ClassAd *rec, bool synch )
{
	int wrote_entry;

	wrote_entry = true;

	if (fp) {
		string buffer;
		unparser.Unparse( buffer, rec );
		if (fprintf(fp, "%s\n", buffer.c_str()) < 0) {
		    wrote_entry = false;
		} else if (synch) {
			if (fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
				wrote_entry = false;
			}
		}
		if (!wrote_entry) {
			CondorErrno = ERR_FILE_WRITE_FAILED;
			CondorErrMsg = "failed to log operation: " + buffer;
		}
	}
	// If there is no log file, we return true. Technically,
	// we haven't written a log file entry, but it wasn't desired
	// anyway, so it's okay.
	return wrote_entry;
}


bool ClassAdCollectionInterface::
ReadLogFile( )
{
    int     fd;

	// open the file and wrap a source around it
    if( ( fd = open( logFileName.c_str( ), O_RDWR | O_CREAT, 0600 ) ) < 0 ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        CondorErrMsg = "failed to open log " + logFileName + " errno=" +
            std::to_string(errno);
        return( false );
    }
    if( ( log_fp = fdopen( fd, "r+" ) ) == NULL ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        CondorErrMsg = "failed to fdopen(" + std::to_string(fd) + ") file ";
        CondorErrMsg += logFileName + " errno=" + std::to_string(errno);
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
        CondorErrMsg += std::to_string( errno );
        return( false );
    }
    if( ( newLog_fp = fdopen( newLog_fd, "r+" ) ) == NULL ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        CondorErrMsg = "when truncating log, failed to fdopen(" + std::to_string(newLog_fd);
        CondorErrMsg += ") file " + tmpLogFileName + " errno=" + std::to_string(errno);
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
        CondorErrMsg = "failed to truncate log: MoveFileEx failed with "
            "error=" + std::to_string( GetLastError() );
        return false;
    }
#else
    if( rename(tmpLogFileName.c_str( ), logFileName.c_str( ) ) < 0 ) {
        CondorErrno = ERR_RENAME_FAILED;
        CondorErrMsg = "failed to truncate log: rename(" + tmpLogFileName +
            "," + logFileName + ") returned errno=" + std::to_string(errno);
        return( false );
    }
#endif

        // re-open new log file
    if( ( log_fp = fopen( logFileName.c_str( ), "a+" ) ) == NULL ) {
        CondorErrno = ERR_LOG_OPEN_FAILED;
        CondorErrMsg = "failed to reopen "+logFileName+", errno="+std::to_string(errno);
        return( false );
    }

    return( true );
}

#ifdef _MSC_VER
#pragma warning(pop) // restore 4996
#endif

static bool string_is_empty(const string &text)
{
	bool is_empty;

	is_empty = true;
	for (unsigned int index = 0; index < text.length(); index++) {
		if (!isspace(text[index])) {
			is_empty = false;
			break;
		}
	}
	return is_empty;
}

}
