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
#include "stream.h"
#include "reli_sock.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "string_list.h"

using namespace std;

// undefine this to get rid of the original (slow) _putClassAd function
//#define ENABLE_V0_PUT_CLASSAD

#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "compat_classad.h"

// local helper functions, options are one or more of PUT_CLASSAD_* flags
int _putClassAd(Stream *sock, classad::ClassAd& ad, int options);
int _putClassAd(Stream *sock, classad::ClassAd& ad, int options, const classad::References &whitelist);
int _mergeStringListIntoWhitelist(StringList & list_in, classad::References & whitelist_out);
#ifdef ENABLE_V0_PUT_CLASSAD
 // these are the 8.2.0 _putClassAd implementations, available for timing comparison
 int _putClassAd_v0( Stream *sock, classad::ClassAd& ad, bool excludeTypes, bool exclude_private );
 int _putClassAd_v0( Stream *sock, classad::ClassAd& ad, bool excludeTypes, bool exclude_private, StringList * whitelist );
 static bool use_v0_put_classad = false;
#endif



static bool publish_server_timeMangled = false;
void AttrList_setPublishServerTimeMangled( bool publish)
{
    publish_server_timeMangled = publish;
}

static const char *SECRET_MARKER = "ZKM"; // "it's a Zecret Klassad, Mon!"

compat_classad::ClassAd *
getClassAd( Stream *sock )
{
	compat_classad::ClassAd *ad = new compat_classad::ClassAd( );
	if( !ad ) { 
		return NULL;
	}
	if( !getClassAd( sock, *ad ) ) {
		delete ad;
		return NULL;
	}
	return ad;	
}

bool getClassAd( Stream *sock, classad::ClassAd& ad )
{
	int 					numExprs;
	MyString				inputLine;

	ad.Clear( );

	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

		// pack exprs into classad
	for( int i = 0 ; i < numExprs ; i++ ) {
		char const *strptr = NULL;
		string buffer;
		if( !sock->get_string_ptr( strptr ) || !strptr ) {
			return( false );	 
		}		

		if(strcmp(strptr,SECRET_MARKER) ==0 ){
			char *secret_line = NULL;
			if( !sock->get_secret(secret_line) ) {
				dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
				break;
			}
			compat_classad::ConvertEscapingOldToNew(secret_line,buffer);
			free( secret_line );
		}
		else {
			compat_classad::ConvertEscapingOldToNew(strptr,buffer);
		}

		// inserts expression at a time
		if ( !ad.Insert(buffer) )
		{
		  dprintf(D_FULLDEBUG, "FAILED to insert %s\n", buffer.c_str() );
		  return false;
		}
		
	}

		// get type info
	if (!sock->get(inputLine)) {
		 dprintf(D_FULLDEBUG, "FAILED to get(inputLine)\n" );
		return false;
	}
	if (inputLine != "" && inputLine != "(unknown type)") {
		if (!ad.InsertAttr("MyType",(string)inputLine.Value())) {
			dprintf(D_FULLDEBUG, "FAILED to insert MyType\n" );
			return false;
		}
	}

	if (!sock->get(inputLine)) {
		 dprintf(D_FULLDEBUG, "FAILED to get(inputLine) 2\n" );
		return false;
	}
	if (inputLine != "" && inputLine != "(unknown type)") {
		if (!ad.InsertAttr("TargetType",(string)inputLine.Value())) {
		dprintf(D_FULLDEBUG, "FAILED to insert TargetType\n" );
			return false;
		}
	}

	return true;
}

int getClassAdNonblocking( ReliSock *sock, classad::ClassAd& ad )
{
	int retval;
	bool read_would_block;
	{
		BlockingModeGuard guard(sock, true);
		retval = getClassAd(sock, ad);
		read_would_block = sock->clear_read_block_flag();
	}
	if (!retval) {
		return 0;
	} else if (read_would_block) {
		return 2;
	}
	return retval;
}

bool
getClassAdNoTypes( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 					numExprs = 0; // Initialization clears Coverity warning
	string					buffer;
	classad::ClassAd		*upd=NULL;
	MyString				inputLine;


	ad.Clear( );

	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

		// pack exprs into classad
	buffer = "[";
	for( int i = 0 ; i < numExprs ; i++ ) {
		if( !sock->get( inputLine ) ) { 
			return( false );	 
		}		

        if(strcmp(inputLine.Value(),SECRET_MARKER) ==0 ){
            char *secret_line = NULL;
            if( !sock->get_secret(secret_line) ) {
                dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
                break;
            }
	    inputLine = secret_line;
	    free( secret_line );
        }

		if ( strncmp( inputLine.Value(), "ConcurrencyLimit.", 17 ) == 0 ) {
			inputLine.setChar( 16, '_' );
		}
		buffer += string(inputLine.Value()) + ";";
	}
	buffer += "]";

		// parse ad
	if( !( upd = parser.ParseClassAd( buffer ) ) ) {
		return( false );
	}

		// put exprs into ad
	ad.Update( *upd );
	delete upd;

	return true;
}

//
//
int _mergeStringListIntoWhitelist(StringList & list_in, classad::References & whitelist_out)
{
	const char * attr;
	list_in.rewind();
	while ((attr = list_in.next())) {
		whitelist_out.insert(attr);
	}
	return (int)whitelist_out.size();
}

// read an attribute from a query ad, and turn it into a classad projection (i.e. a set of attributes)
// the projection attribute can be a string, or (if allow_list is true) a classad list of strings.
// returns:
//    -1 if projection does not evaluate
//    -2 if projection does not convert to string
//    0  if no projection or projection is valid but empty
//    1  the projection is non-empty
//
int mergeProjectionFromQueryAd(classad::ClassAd & queryAd, const char * attr_projection, classad::References & projection, bool allow_list /*= false*/)
{
	if ( ! queryAd.Lookup(attr_projection))
		return 0; // no projection

	classad::Value value;
	if ( ! queryAd.EvaluateAttr(attr_projection, value)) {
		return -1;
	}

	classad::ExprList *list = NULL;
	if (allow_list && value.IsListValue(list)) {
		for (classad::ExprList::const_iterator it = list->begin(); it != list->end(); it++) {
			std::string attr;
			if (!(*it)->Evaluate(value) || !value.IsStringValue(attr)) {
				return -2;
			}
			projection.insert(attr);
		}
		return projection.empty() ? 0 : 1;
	}

	std::string proj_list;
	if (value.IsStringValue(proj_list)) {
		StringTokenIterator list(proj_list);
		const std::string * attr;
		while ((attr = list.next_string())) { projection.insert(*attr); }
	} else {
		return -2;
	}

	return projection.empty() ? 0 : 1;
}


/* 
 * It now prints chained attributes. Or it should.
 * 
 * It should add the server_time attribute if it's
 * defined.
 *
 * It should convert default IPs to SocketIPs.
 *
 * It should also do encryption now.
 */
int putClassAd ( Stream *sock, classad::ClassAd& ad )
{
	int options = 0;

#ifdef ENABLE_V0_PUT_CLASSAD
	if (use_v0_put_classad) {
		return _putClassAd_v0(sock, ad, false, false, NULL);
	}
#endif

	return _putClassAd(sock, ad, options);
}

#if 0
/*
 * Put the ClassAd onto the wire in a non-blocking manner.
 * Return codes:
 * - If the network would have blocked (meaning the socket is buffering the add internally),
 *   then this returns 2.  Callers should stop sending ads, register the socket with DC for
 *   write, and wait for a callback.
 * - On success, this returns 1; this indicates that further ClassAds can be sent to this ReliSock.
 * - On permanent failure, this returns 0.
 */
PRAGMA_REMIND("TJ: kill this off.")
int putClassAdNonblocking(ReliSock *sock, classad::ClassAd& ad, bool exclude_private, StringList *attr_whitelist )
{
	int options = 0;
	if (exclude_private) { options |= PUT_CLASSAD_NO_PRIVATE; }

	int retval;
	bool backlog;
	{
		BlockingModeGuard guard(sock, true);
	#ifdef ENABLE_V0_PUT_CLASSAD
		if (use_v0_put_classad) {
			retval = _putClassAd_v0(sock, ad, false, exclude_private, attr_whitelist);
		} else
	#endif
		if (attr_whitelist) {
			classad::References attrs;
			_mergeStringListIntoWhitelist(*attr_whitelist, attrs);
			retval = _putClassAd(sock, ad, options, attrs);
		} else {
			retval = _putClassAd(sock, ad, options);
		}
		backlog = sock->clear_backlog_flag();
	}
	if (!retval) {
		return 0;
	} else if (backlog) {
		return 2;
	}
	return retval;
}

int
putClassAdNoTypes ( Stream *sock, classad::ClassAd& ad )
{
#ifdef ENABLE_V0_PUT_CLASSAD
	if (use_v0_put_classad) {
		return _putClassAd_v0(sock, ad, true, false);
	}
#endif
    return _putClassAd(sock, ad, PUT_CLASSAD_NO_TYPES);
}
#endif

#ifdef ENABLE_V0_PUT_CLASSAD
// these are here for timing comparison
int _putClassAd_v0( Stream *sock, classad::ClassAd& ad, bool excludeTypes, bool exclude_private )
{
	classad::ClassAdUnParser	unp;
	std::string					buf;
    bool send_server_time = false;

	unp.SetOldClassAd( true, true );

	int numExprs=0;

    classad::AttrList::const_iterator itor;
    classad::AttrList::const_iterator itor_end;

    bool haveChainedAd = false;
    
    classad::ClassAd *chainedAd = ad.GetChainedParentAd();
    
    if(chainedAd){
        haveChainedAd = true;
    }

    for(int pass = 0; pass < 2; pass++){

        /* 
        * Count the number of chained attributes on the first
        *   pass (if any!), then the number of attrs in this classad on
        *   pass number 2.
        */
        if(pass == 0){
            if(!haveChainedAd){
                continue;
            }
            itor = chainedAd->begin();
			itor_end = chainedAd->end();
        }
        else {
            itor = ad.begin();
			itor_end = ad.end();
        }

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;

            if(!exclude_private ||
			   !compat_classad::ClassAdAttributeIsPrivate(attr.c_str()))
            {
                if(excludeTypes)
                {
                    if(strcasecmp( ATTR_MY_TYPE, attr.c_str() ) != 0 &&
                        strcasecmp( ATTR_TARGET_TYPE, attr.c_str() ) != 0)
                    {
                        numExprs++;
                    }
                }
                else { numExprs++; }
            }
			if ( strcasecmp( ATTR_CURRENT_TIME, attr.c_str() ) == 0 ) {
				numExprs--;
			}
        }
    }

    if( publish_server_timeMangled ){
        //add one for the ATTR_SERVER_TIME expr
        numExprs++;
        send_server_time = true;
    }

	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
    
    for(int pass = 0; pass < 2; pass++){
        if(pass == 0) {
            /* need to copy the chained attrs first, so if
             *  there are duplicates, the non-chained attrs
             *  will override them
             */
            if(!haveChainedAd){
                continue;
            }
            itor = chainedAd->begin();
			itor_end = chainedAd->end();
        } 
        else {
            itor = ad.begin();
			itor_end = ad.end();
        }

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;
			classad::ExprTree const *expr = itor->second;

			if(strcasecmp(ATTR_CURRENT_TIME,attr.c_str())==0) {
				continue;
			}
            if(exclude_private && compat_classad::ClassAdAttributeIsPrivate(attr.c_str())){
                continue;
            }

            if(excludeTypes){
                if(strcasecmp( ATTR_MY_TYPE, attr.c_str( ) ) == 0 || 
				   strcasecmp( ATTR_TARGET_TYPE, attr.c_str( ) ) == 0 )
				{
                    continue;
                }
            }

			buf = attr;
            buf += " = ";
            unp.Unparse( buf, expr );

            ConvertDefaultIPToSocketIP(attr.c_str(),buf,*sock);

            if( ! sock->prepare_crypto_for_secret_is_noop() &&
				compat_classad::ClassAdAttributeIsPrivate(attr.c_str()))
			{
                sock->put(SECRET_MARKER);

                sock->put_secret(buf.c_str());
            }
            else if (!sock->put(buf.c_str()) ){
                return false;
            }
        }
    }

    if(send_server_time) {
        //insert in the current time from the server's (Schedd) point of
        //view. this is used so condor_q can compute some time values 
        //based upon other attribute values without worrying about 
        //the clocks being different on the condor_schedd machine
        // -vs- the condor_q machine

        char* serverTimeStr;
        serverTimeStr = (char *) malloc(strlen(ATTR_SERVER_TIME)
                                        + 3     //for " = "
                                        + 12    // for integer
                                        +1);    //for null termination
        ASSERT( serverTimeStr );
        sprintf(serverTimeStr, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL) );
        if(!sock->put(serverTimeStr)){
            free(serverTimeStr);
            return 0;
        }
        free(serverTimeStr);
    }

    //ok, so the name of the bool doesn't really work here. It works
    //  in the other places though.
    if(!excludeTypes)
    {
        // Send the type
        if (!ad.EvaluateAttrString(ATTR_MY_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }

        if (!ad.EvaluateAttrString(ATTR_TARGET_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }
    }

	return true;
}

int _putClassAd_v0( Stream *sock, classad::ClassAd& ad, bool excludeTypes, bool exclude_private, StringList *attr_whitelist )
{
	if ( ! attr_whitelist) {
		return _putClassAd_v0(sock, ad, excludeTypes, exclude_private);
	}

	classad::ClassAdUnParser	unp;
	std::string					buf;
    bool send_server_time = false;

	unp.SetOldClassAd( true, true );

	int numExprs=0;

	numExprs += attr_whitelist->number();

    if( publish_server_timeMangled ){
        //add one for the ATTR_SERVER_TIME expr
        numExprs++;
        send_server_time = true;
    }

	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
    
	if( attr_whitelist ) {
		attr_whitelist->rewind();
		char const *attr;
		while( (attr=attr_whitelist->next()) ) {
			classad::ExprTree const *expr = ad.Lookup(attr);
			buf = attr;
			buf += " = ";
            if( !expr || (exclude_private && compat_classad::ClassAdAttributeIsPrivate(attr)) )
			{
				buf += "undefined";
            }
			else {
				unp.Unparse( buf, expr );
			}
            ConvertDefaultIPToSocketIP(attr,buf,*sock);

            if( ! sock->prepare_crypto_for_secret_is_noop() &&
				compat_classad::ClassAdAttributeIsPrivate(attr) )
			{
                sock->put(SECRET_MARKER);

                sock->put_secret(buf.c_str());
            }
            else if (!sock->put(buf.c_str()) ){
                return false;
            }
		}
	}

    if(send_server_time) {
        //insert in the current time from the server's (Schedd) point of
        //view. this is used so condor_q can compute some time values 
        //based upon other attribute values without worrying about 
        //the clocks being different on the condor_schedd machine
        // -vs- the condor_q machine

        char* serverTimeStr;
        serverTimeStr = (char *) malloc(strlen(ATTR_SERVER_TIME)
                                        + 3     //for " = "
                                        + 12    // for integer
                                        +1);    //for null termination
        ASSERT( serverTimeStr );
        sprintf(serverTimeStr, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL) );
        if(!sock->put(serverTimeStr)){
            free(serverTimeStr);
            return 0;
        }
        free(serverTimeStr);
    }

    //ok, so the name of the bool doesn't really work here. It works
    //  in the other places though.
    if(!excludeTypes)
    {
        // Send the type
        if (!ad.EvaluateAttrString(ATTR_MY_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }

        if (!ad.EvaluateAttrString(ATTR_TARGET_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }
    }

	return true;
}
#endif // ENABLE_V0_PUT_CLASSAD

int putClassAd (Stream *sock, classad::ClassAd& ad, int options, const classad::References * whitelist /*=NULL*/)
{
	int retval = 0;
	classad::References expanded_whitelist; // in case we need to expand the whitelist

	bool expand_whitelist = ! (options & PUT_CLASSAD_NO_EXPAND_WHITELIST);
	if (whitelist && expand_whitelist) {
		//PRAGMA_REMIND("need a version of GetInternalReferences that understands that MY is an alias for SELF")
		ad.InsertAttr("MY","SELF");
		for (classad::References::const_iterator attr = whitelist->begin(); attr != whitelist->end(); ++attr) {
			ExprTree * tree = ad.Lookup(*attr);
			if (tree) {
				expanded_whitelist.insert(*attr); // the node exists, so add it to the final whitelist
				if (tree->GetKind() != ExprTree::LITERAL_NODE) {
					ad.GetInternalReferences(tree, expanded_whitelist, false);
				}
			}
		}
		ad.Delete("MY");
		classad::References::iterator my = expanded_whitelist.find("MY");
		if (my != expanded_whitelist.end()) { expanded_whitelist.erase(my); }
		whitelist = &expanded_whitelist;
	}

	bool non_blocking = (options & PUT_CLASSAD_NON_BLOCKING) != 0;
	ReliSock* rsock = static_cast<ReliSock*>(sock);
	if (non_blocking && rsock)
	{
		BlockingModeGuard guard(rsock, true);
		if (whitelist) {
			retval = _putClassAd(sock, ad, options, *whitelist);
		} else {
			retval = _putClassAd(sock, ad, options);
		}
		bool backlog = rsock->clear_backlog_flag();
		if (retval && backlog) { retval = 2; }
	}
	else // normal blocking mode put
	{
		if (whitelist) {
			retval = _putClassAd(sock, ad, options, *whitelist);
		} else {
			retval = _putClassAd(sock, ad, options);
		}
	}
	return retval;
}

// helper function for _putClassAd
static int _putClassAdTrailingInfo(Stream *sock, classad::ClassAd& ad, bool send_server_time, bool excludeTypes)
{
    if (send_server_time)
    {
        //insert in the current time from the server's (Schedd) point of
        //view. this is used so condor_q can compute some time values
        //based upon other attribute values without worrying about
        //the clocks being different on the condor_schedd machine
        // -vs- the condor_q machine

        static const char fmt[] = ATTR_SERVER_TIME " = %ld";
        char buf[sizeof(fmt) + 12]; //+12 for time value
        sprintf(buf, fmt, (long)time(NULL));
        if (!sock->put(buf)) {
            return false;
        }
    }

    //ok, so the name of the bool doesn't really work here. It works
    //  in the other places though.
    if (!excludeTypes)
    {
        std::string buf;
        // Send the type
        if (!ad.EvaluateAttrString(ATTR_MY_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }

        if (!ad.EvaluateAttrString(ATTR_TARGET_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }
    }

	return true;
}

int _putClassAd( Stream *sock, classad::ClassAd& ad, int options)
{
	bool excludeTypes = (options & PUT_CLASSAD_NO_TYPES) == PUT_CLASSAD_NO_TYPES;
	bool exclude_private = (options & PUT_CLASSAD_NO_PRIVATE) == PUT_CLASSAD_NO_PRIVATE;

	classad::ClassAdUnParser	unp;
	std::string					buf;
	bool send_server_time = false;

	unp.SetOldClassAd( true, true );

	int numExprs=0;

	classad::AttrList::const_iterator itor;
	classad::AttrList::const_iterator itor_end;

	bool haveChainedAd = false;

	classad::ClassAd *chainedAd = ad.GetChainedParentAd();
	if(chainedAd){
		haveChainedAd = true;
	}

	for(int pass = 0; pass < 2; pass++){

		/*
		* Count the number of chained attributes on the first
		*   pass (if any!), then the number of attrs in this classad on
		*   pass number 2.
		*/
		if(pass == 0){
			if(!haveChainedAd){
				continue;
			}
			itor = chainedAd->begin();
			itor_end = chainedAd->end();
		}
		else {
			itor = ad.begin();
			itor_end = ad.end();
		}

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;

			if(!exclude_private ||
				!compat_classad::ClassAdAttributeIsPrivate(attr.c_str()))
			{
				if(excludeTypes)
				{
					if(strcasecmp( ATTR_MY_TYPE, attr.c_str() ) != 0 &&
						strcasecmp( ATTR_TARGET_TYPE, attr.c_str() ) != 0)
					{
						numExprs++;
					}
				}
				else { numExprs++; }
			}
			if ( strcasecmp( ATTR_CURRENT_TIME, attr.c_str() ) == 0 ) {
				numExprs--;
			}
		}
	}

	if( publish_server_timeMangled ){
		//add one for the ATTR_SERVER_TIME expr
		numExprs++;
		send_server_time = true;
	}

	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}

	for(int pass = 0; pass < 2; pass++){
		if(pass == 0) {
			/* need to copy the chained attrs first, so if
				*  there are duplicates, the non-chained attrs
				*  will override them
				*/
			if(!haveChainedAd){
				continue;
			}
			itor = chainedAd->begin();
			itor_end = chainedAd->end();
		} 
		else {
			itor = ad.begin();
			itor_end = ad.end();
		}

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;
			classad::ExprTree const *expr = itor->second;

			if(strcasecmp(ATTR_CURRENT_TIME,attr.c_str())==0) {
				continue;
			}
			if(exclude_private && compat_classad::ClassAdAttributeIsPrivate(attr.c_str())){
				continue;
			}

			if(excludeTypes){
				if(strcasecmp( ATTR_MY_TYPE, attr.c_str( ) ) == 0 || 
				   strcasecmp( ATTR_TARGET_TYPE, attr.c_str( ) ) == 0 )
				{
					continue;
				}
			}

			buf = attr;
			buf += " = ";
			unp.Unparse( buf, expr );

			ConvertDefaultIPToSocketIP(attr.c_str(),buf,*sock);

			if( ! sock->prepare_crypto_for_secret_is_noop() &&
				compat_classad::ClassAdAttributeIsPrivate(attr.c_str()))
			{
				sock->put(SECRET_MARKER);

				sock->put_secret(buf.c_str());
			}
			else if (!sock->put(buf.c_str()) ){
				return false;
			}
		}
	}

	return _putClassAdTrailingInfo(sock, ad, send_server_time, excludeTypes);
}

int _putClassAd( Stream *sock, classad::ClassAd& ad, int options, const classad::References &whitelist)
{
	bool excludeTypes = (options & PUT_CLASSAD_NO_TYPES) == PUT_CLASSAD_NO_TYPES;
	bool exclude_private = (options & PUT_CLASSAD_NO_PRIVATE) == PUT_CLASSAD_NO_PRIVATE;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );

	classad::References blacklist;
	for (classad::References::const_iterator attr = whitelist.begin(); attr != whitelist.end(); ++attr) {
		if ( ! ad.Lookup(*attr) || (exclude_private && compat_classad::ClassAdAttributeIsPrivate(attr->c_str()))) {
			blacklist.insert(*attr);
		}
	}

	int numExprs = whitelist.size() - blacklist.size();

	bool send_server_time = false;
	if( publish_server_timeMangled ){
		//add one for the ATTR_SERVER_TIME expr
		// if its in the whitelist but not the blacklist, add it to the blacklist instead
		// since its already been counted in that case.
		if (whitelist.find(ATTR_SERVER_TIME) != whitelist.end() && 
			blacklist.find(ATTR_SERVER_TIME) == blacklist.end()) {
			blacklist.insert(ATTR_SERVER_TIME);
		} else {
			++numExprs;
		}
		send_server_time = true;
	}


	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}

	std::string buf;
	for (classad::References::const_iterator attr = whitelist.begin(); attr != whitelist.end(); ++attr) {

		if (blacklist.find(*attr) != blacklist.end())
			continue;

		classad::ExprTree const *expr = ad.Lookup(*attr);
		buf = *attr;
		buf += " = ";
		unp.Unparse( buf, expr );
		ConvertDefaultIPToSocketIP(attr->c_str(),buf,*sock);

		if ( ! sock->prepare_crypto_for_secret_is_noop() &&
			compat_classad::ClassAdAttributeIsPrivate(attr->c_str()) )
		{
			sock->put(SECRET_MARKER);
			sock->put_secret(buf.c_str());
		}
		else if ( ! sock->put(buf.c_str()) ){
			return false;
		}
	}

	return _putClassAdTrailingInfo(sock, ad, send_server_time, excludeTypes);
}

bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::Value* v)
{
    return EvalTree(eTree, mine, NULL, v);
}


bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::ClassAd* target, classad::Value* v)
{
    if(!mine)
    {
        return false;
    }
    const classad::ClassAd* tmp = eTree->GetParentScope(); 
    eTree->SetParentScope(mine);

    if(target)
    {
        classad::MatchClassAd mad(mine,target);

        bool rval = eTree->Evaluate(*v);

        mad.RemoveLeftAd( );
        mad.RemoveRightAd( );
        
        //restore the old scope
        eTree->SetParentScope(tmp);

        return rval;
    }


    //restore the old scope
    eTree->SetParentScope(tmp);

    return eTree->Evaluate(*v);
}
