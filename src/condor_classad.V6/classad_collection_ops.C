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
#include "classad_collection_ops.h"
#include "classad_collection.h"

static char *_FileName_ = __FILE__;

namespace classad {

//------------------------------------------------------------------------
// Add new class ad
//------------------------------------------------------------------------

LogCollNewClassAd::LogCollNewClassAd(const char* k, ClassAd* ad)
{
  op_type = CondorLogOp_CollNewClassAd;
  Executed=false;
  key=NULL;
  Ad=NULL;
  if (k) key=strdup(k);
  if (ad) Ad = ad;
}

LogCollNewClassAd::~LogCollNewClassAd()
{
  if (key) free(key);
  if (!Executed && Ad) delete Ad;
}

bool LogCollNewClassAd::Play(void *data_structure)
{
  ClassAdCollection* coll=(ClassAdCollection*) data_structure;
  if( !coll->PlayNewClassAd(key,Ad) ) return( false );
  Executed=true;
  return( true );
}

bool LogCollNewClassAd::ReadBody(FILE* fp)
{
  if (key) { 
    EXCEPT("Shouldn't have happened\n");
    free(key);
    key=NULL;
  }
  if (!readword(fp, key)) return false;

  // the rest of the line is a class-ad

  if (Ad) { 
    EXCEPT("Shouldn't have happened\n");
    free(Ad);
    Ad=NULL;
  }
  Source src;
  src.SetSource( fp );
  src.SetSentinelChar( '\n' );
  if( !src.ParseClassAd( Ad ) ) return false;

  return true;
}

bool LogCollNewClassAd::WriteBody(FILE* fp)
{
  if ( fprintf( fp, "%s ", key ) < 0 ) return false;

  // the expression goes on the rest of the line; WriteTail puts \n

  Sink sink;
  sink.SetSink( fp );
  sink.SetTerminalChar( ' ' );
  if( !Ad->ToSink( sink ) ) return false;
  sink.FlushSink( );

  Executed=true;
  return true;

}

//------------------------------------------------------------------------
// Add change class ad
//------------------------------------------------------------------------

LogCollUpdateClassAd::LogCollUpdateClassAd(const char* k, ClassAd* ad) : LogCollNewClassAd(k,ad) 
{
  op_type = CondorLogOp_CollUpdateClassAd;
}

LogCollUpdateClassAd::~LogCollUpdateClassAd()
{
	if( key ) {
		free( key );
		key = NULL;
	}
	if( Ad ) {
		delete Ad;
		Ad = NULL;
	}
}

bool LogCollUpdateClassAd::Play(void *data_structure)
{
  ClassAdCollection* coll=(ClassAdCollection*) data_structure;
  return( coll->PlayUpdateClassAd(key,Ad) );
}

//------------------------------------------------------------------------
// Modify class Ad
//------------------------------------------------------------------------
LogCollModifyClassAd::LogCollModifyClassAd(const char*k, ClassAd*ad) :
	LogCollNewClassAd( k, ad )
{
	op_type = CondorLogOp_CollModifyClassAd;
}

LogCollModifyClassAd::~LogCollModifyClassAd()
{
	if( key ) {
		free( key );
		key = NULL;
	}
	if( Ad ) {
		delete Ad;
		Ad = NULL;
	}
}

bool LogCollModifyClassAd::Play(void *data_structure)
{
	ClassAdCollection* coll=(ClassAdCollection*) data_structure;
	return( coll->PlayModifyClassAd(key, Ad) );
}

//------------------------------------------------------------------------
// Delete class Ad
//------------------------------------------------------------------------

LogCollDestroyClassAd::LogCollDestroyClassAd(const char* k)
{
  op_type = CondorLogOp_CollDestroyClassAd;
  key=NULL;
  if (k) key=strdup(k);
}

LogCollDestroyClassAd::~LogCollDestroyClassAd()
{
  if (key) free(key);
}

bool LogCollDestroyClassAd::Play(void *data_structure)
{
  ClassAdCollection* coll=(ClassAdCollection*) data_structure;
  return( coll->PlayDestroyClassAd(key) );
}

bool LogCollDestroyClassAd::ReadBody(FILE* fp)
{
  if (key) free(key);
  if (!readword(fp, key)) return false;

  return true;
}

bool LogCollDestroyClassAd::WriteBody(FILE* fp)
{
  if( fprintf( fp, "%s ", key ) < 0 ) return false;
  return true;
}

} // namespace classad
