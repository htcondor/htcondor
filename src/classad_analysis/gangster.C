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
#include "gangster.h"

//-----------------------------------------------------------------------------
int Gangster::nextContextNum = 0;

//-----------------------------------------------------------------------------
// globals
classad::ClassAd			bundle;
GMRMap			offerGMRs;
classad::ClassAdParser	adParser;
int				matchesTested;

//-----------------------------------------------------------------------------
// Gangster implementation

Gangster::Gangster( Gangster *parent, int dockNum )
{
	char buffer[32];

	parentLinked = -1;
	parentGangster = parent;
	parentDockNum  = dockNum;
	contextNum = nextContextNum++;
	context = new classad::ClassAd( );
	sprintf( buffer, "_ctx_%d", contextNum );
	bundle.Insert( buffer, context );
#if defined(DEBUG)
	printf( "Created Gangster with contextNum %d (in ctor)\n", contextNum );
#endif
}


Gangster::~Gangster( )
{
	char buffer[32];
	sprintf( buffer, "_ctx_%d", contextNum );
	bundle.Delete( buffer );
#if defined(DEBUG)
	printf( "Deleted Gangster with contextNum %d (in dtor)\n", contextNum );
#endif
}


void
Gangster::assign( GMR *tempGmr )
{
	GMR::Ports::iterator	gmrpItr;
	Dock					di;

#if defined(DEBUG)
	printf( "Gangster %d: Assigned ad %d, replacing %d\n", contextNum,
		tempGmr->key, gmr ? gmr->key : -1 );
#endif

		// do cleanups here??
	gmr = tempGmr;
	parentLinked = -1;

		// establish/modify context
	context->Remove( "_ad" ); // don't want the ad to be deleted
	context->Insert( "_ad", gmr->parentAd );

		// insert dock info 
	docks.clear( );
	di.boundGangster = NULL;
	di.lastKey = -1;
	di.boundDock = -1;
	di.satisfied = false;
	for( gmrpItr=gmr->ports.begin(); gmrpItr!=gmr->ports.end(); gmrpItr++ ){
		di.port = &*gmrpItr;
		docks.push_back( di );
	}
}


void
Gangster::retire( bool finish )
{
	Docks::iterator	ditr;

		// already retired?
	if( !gmr ) return;

#if defined(DEBUG)
	printf( "Retiring Gangster %d: \n", contextNum );
#endif

		// reintroduce non-root gmrs to offer pool
	if( gmr && !finish && parentGangster ) {
#if defined(DEBUG)
		printf( "  Returning to ad %d to offer pool\n", gmr->key );
#endif
		offerGMRs[gmr->key] = gmr;
	}

		// clean out node information
	context->Remove( "_ad" );
	context->Clear( );
#if defined(DEBUG)
	printf( "  Removed ad and cleared out context\n" );
	printf( "  Removing child gangsters\n" );
#endif

		// remove child gangsters
	for( ditr=docks.begin( ); ditr!=docks.end( ); ditr++ ) {
		if( ditr->boundGangster && ditr->boundGangster != parentGangster ) {
			ditr->boundGangster->retire( finish );
			delete ditr->boundGangster;
		}
	}
	docks.clear( );

	if( finish && gmr ) { 
		if( parentGangster != NULL ) {
#if defined(DEBUG)
			printf( "  Removing from offer pool\n" );
#endif
			offerGMRs.erase( gmr->key );
		}
		delete gmr;
	}
	gmr = NULL;
	parentLinked = -1;
}


void
Gangster::reset( )
{
	retire( true );
	nextContextNum = 1;
}


bool
Gangster::setupMatchBindings( int dockNum ) 
{
	char		buffer[64];
	GMR::Port	*pport;

		// do sanity checks
	if( !parentGangster ) return( false );
	pport = parentGangster->docks[parentDockNum].port;

		// establish bindings in self context
	sprintf( buffer, "_ctx_%d._ad.ports[%d]", parentGangster->contextNum, 
		parentDockNum );
	if( !( context->Insert(docks[dockNum].port->label,
			adParser.ParseExpression(buffer)))) {
		printf( "Failed to insert binding %s=%s\n", pport->label.c_str(),
			buffer );		
		return( false );
	}
		// establish binding in parent context
	sprintf( buffer, "_ctx_%d._ad.ports[%d]", contextNum, dockNum );
	if( !parentGangster->context->Insert(pport->label, 
			adParser.ParseExpression(buffer)) ){
		printf( "Failed to insert binding %s=%s\n", pport->label.c_str(),
			buffer );		
		return( false );
	}

		// done
	return( true );
}


void
Gangster::printKeys( )
{
	if( !gmr ) return;
	printf( "%d( ", gmr->key );

	for( Docks::iterator ditr=docks.begin(); ditr != docks.end(); ditr++) {
		if( ditr->boundGangster != parentGangster ) {
			ditr->boundGangster->printKeys( );
		} else {
			printf( "^ " );
		}
	}
	printf( ")" );
}


void
Gangster::teardownMatchBindings( int dockNum )
{
		// delete bindings from self's context and from parent context
	context->Delete( docks[dockNum].port->label );
	if( parentGangster ) {
		parentGangster->context->Delete(
			parentGangster->docks[parentDockNum].port->label);
	}
}


void
Gangster::testMatch( int dockNum, classad::Value &val )
{
	classad::Value	val1, val2;

	matchesTested++;

		// test dock's requirements and parent dock's requirements
	val.SetErrorValue( );
#if defined(DEBUG)
	printf( "---\n" );
	bundle.Puke( );
	docks[dockNum].port->portAd->Puke();
	parentGangster->docks[parentDockNum].port->portAd->Puke();
	printf( "---\n" );
#endif 
	if( !docks[dockNum].port->portAd->EvaluateAttr( "Requirements", val1 ) ||
			!parentGangster->docks[parentDockNum].port->portAd->EvaluateAttr(
				"Requirements", val2 ) ) {
		return;
	}
		// combine both outcomes
	classad::Operation::Operate( classad::Operation::LOGICAL_AND_OP,
								 val1, val2, val );
}

void
Gangster::testOneWayMatch( int dockNum, classad::Value &val )
{
		// test dock's requirements and parent dock's requirements
	val.SetErrorValue( );
#if defined(DEBUG)
	classad::PrettyPrint pp;
	std::string buffer = "";
	printf( "---\n" );
	pp.Unparse( buffer, &bundle );
	pp.Unparse( buffer, docks[dockNum].port->portAd );
	if( parentGangster ) {
		pp.Unparse( buffer, parentGangster->docks[parentDockNum].port->portAd);
	}
	cout << buffer;
	printf( "---\n" );
#endif 
	if( !docks[dockNum].port->portAd->EvaluateAttr( "Requirements", val ) ) {
		return;
	}
}

bool
Gangster::match( bool backtrack )
{
	Gangster::Docks::iterator	ditr;
	std::string					bindings;
	int							dockNum;
	classad::Value						val;

		// if we're backtracking, we need to find the last port satisfied
		// otherwise, we start from the beginning and work from the first
		// port that is not satisfied
	if( backtrack ) {
		dockNum = docks.size( ) - 1;
		ditr = docks.begin( ) + dockNum;
	} else {
		ditr = docks.begin( );
		dockNum = 0;
	}

	while( 1 ) {
			// (1) at the end *and* (2) not backtracking *and* (3) linked
			// to the parent (or have no parent), we're done
		if( (unsigned)dockNum >= docks.size( ) && !backtrack ) {
			if( parentLinked >= 0 || parentGangster == NULL ) return( true );
				// otherwise, we failed;  cleanup and return failure
			retire( );
			return( false );
		}

			// if we're before the start and we are backtracking, we failed
		if( dockNum < 0 && backtrack ) {
				// cleanup and return failure
			retire( );
			return( false );
		}

		/*
			// if we're not backtracking and we found a satisfied port, find
			// a succeeding port to satisfy
		if( ditr->satisfied && !backtrack ) {
			ditr++;
			dockNum++;
			continue;
		}

			// if we're backtracking and we found an unsatisfied port, find 
			// a preceeding port (that is satisfied) to backtrack from
		if( !ditr->satisfied && backtrack ) {
			ditr--;
			dockNum--;
			continue;
		}
		*/

			// if we've found the parentLinked dock and we are not
			// backtracking, move over to the next dock ...
		if( dockNum == parentLinked && !backtrack ) {
			ditr++;
			dockNum++;
			continue;
		}

			// if we've found the parentLinked dock and we are backtracking
			// skip the parentLink, but make the binding "weak"
			// (if the parentLink is the first port, we must check if some
			// succeeding port can maintain the parentLink)
		if( dockNum == parentLinked && backtrack ) {
			if( ditr == docks.begin( ) ) {
					// Optimization:  if there is no later port, no parent 
					// link can be established, so retire 
				if( (unsigned)dockNum == docks.size( )-1 ) {
					retire( );
					return( false );
				}

					// float the parentLink
				parentLinked = -1;
				ditr->lastKey = -1;
				ditr->boundGangster = NULL;
				ditr->boundDock = -1;
				ditr->satisfied = false;

					// fill in this dock with a regular request
				if( !fillDock( 0 , false ) ) {
					retire( );
					return( false );
				} else {
					ditr++;
					dockNum++;
					backtrack = false;
				}
			} else {
				ditr--;
				dockNum--;
				ditr->satisfied = false;
			}
			continue;
		}

			// if we've found an unsatisfied port and we're not parentLinked
			// and we're not the gang root, check if this port works as the 
			// parent link
		if(!ditr->satisfied && !backtrack && parentLinked<0 && parentGangster){
			bool b;
			if( !setupMatchBindings( dockNum ) ) {
				printf( "Failed to setup bindings; aborting this match\n" );
				return( false );
			}

				// do they match? (don't tolerate UNDEFINED)
			testMatch( dockNum, val );
			if( val.IsBooleanValue( b ) && b ) {
					// yes ... setup as parent link
				ditr->satisfied = true;
				ditr->boundDock = parentDockNum;
				ditr->lastKey   = parentGangster->gmr->key;
				if(ditr->boundGangster && ditr->boundGangster!=parentGangster){
					ditr->boundGangster->retire( );
					delete ditr->boundGangster;
				}
				ditr->boundGangster = parentGangster;
				parentLinked    = dockNum;

				ditr++;
				dockNum++;

				continue;
			}
		}


			// if we've found the weak linked parentLink port and we're not 
			// backtracking, ensure that the parentLink is satisfied
		if( dockNum == parentLinked && !ditr->satisfied && !backtrack ) {
				// check requirements; if requirements are not satisfied, break 
				// parentLink with this dock --- we need to see if some 
				// succeeding port can maintain parentLink
			bool	b;

			testMatch( dockNum, val );
			if( val.IsBooleanValue( b ) && b ) {
					// worked!  convert weak link to a strong link
				ditr->satisfied = true;
				ditr++;
				dockNum++;
				continue;
			} else {
					// did not work; break link and hope a succeeding port can
					// set up a parent link
				parentLinked = -1;
				ditr->boundGangster = NULL;
				ditr->boundDock = -1;

				continue;
			}
		}


			// if the port is not satisfied and we're not backtracking, fill it
		if( !ditr->satisfied && !backtrack ) {
			if( !fillDock( dockNum, backtrack ) ) {
					// if the dock could not be filled, backtrack
				backtrack = true;
				ditr--;
				dockNum--;
				continue;
			} else {
					// great --- the port got filled; try the next one
				ditr++;
				dockNum++;
				continue;
			}
		}

			// if the port is satisfied and we're backtracking, refill it
		if( ditr->satisfied && backtrack ) {
			if( !fillDock( dockNum, backtrack ) ) {
					// didn't work; vacate dock and retreat further
				ditr--;
				dockNum--;
				continue;
			} else {
					// worked!  continue forward
				backtrack = false;
				ditr++;
				dockNum++;
				continue;
			}
		}
	}

		// will not reach here; for c++
	return( false );
}


bool
Gangster::fillDock( int dockNum, bool backtrack )
{
	GMRMap::iterator			gmrItr;
	GMR							*gmrCand;
	Gangster::Docks::iterator	cditr;		// iterator over candidate's docks
	int							cdock;		// dock number of candidate
	classad::Value						val;
	bool						b;
	Gangster					*gangster;

		// make a Gangster for the dock; or use the one that's already there
		// (if we're backtracking there better already be a gangster)
	if( docks[dockNum].boundGangster || backtrack ) {
		gangster = docks[dockNum].boundGangster;
	} else {
		gangster = new Gangster( this, dockNum );
		docks[dockNum].boundGangster = gangster;
	}
	if( !gangster ) {
		printf( "Error:  Failed to find/create gangster\n" );
		return( false );
	}

		// if backtracking, first try a recursive match in backtrack mode
	if( backtrack ) {
		if( gangster->match( true ) ) {
				// the recursive match worked; done
			return( true );
		} 
	}

		// start iterating from after the last key considered; if starting
		// off, lastKey will be -1, so upper_bound() should return begin()
	gmrItr = offerGMRs.upper_bound( docks[dockNum].lastKey );
	for( ; gmrItr!=offerGMRs.end( ); gmrItr++ ) {

			// consider the gmr candidate
		gmrCand = gmrItr->second;
		gangster->assign( gmrCand );

			// check if any ports in the candidate GMR can dock
		for(cdock=0,cditr=gangster->docks.begin(); cditr!=gangster->docks.end();
				cditr++, cdock++ ) {

				// setup bindings
			if( !gangster->setupMatchBindings( cdock ) ) {
				printf( "Error:  Failed to setup bindings when considering"
					" candidate %d; ignoring candidate\n", gmrItr->first );
				break;
			}

				// test the match
			gangster->testMatch( cdock, val );
			if( !val.IsUndefinedValue( ) && (!val.IsBooleanValue(b) || !b) ) {
					// not UNDEFINED or TRUE; try another
				gangster->teardownMatchBindings( cdock );
				continue; // check next port
			}

				// Optimization: cannot be UNDEFINED if its the first port
			if( val.IsUndefinedValue( ) && cdock == 0 ) {
				gangster->teardownMatchBindings( cdock );
				continue;
			}

				// dock to parent
			gangster->parentLinked = cdock;
			cditr->satisfied = val.IsBooleanValue( ); // not satisfied if UNDEF
			cditr->lastKey = -1;         // irrelevant, so pack a -1
			cditr->boundGangster = this; // same as parentGangster
			cditr->boundDock = dockNum;  // same as parentDockNum

				// dock parent to new gangster
			docks[dockNum].boundGangster = gangster;
			docks[dockNum].boundDock = cdock;
			docks[dockNum].satisfied = cditr->satisfied;
			docks[dockNum].lastKey   = gmrItr->first;

				// remove candidate GMR from outstanding offers list
			offerGMRs.erase( gmrItr );

				// recursively satisfy this gangster
			if( gangster->match( false ) ) {
				return( true );
			}

				// could not match --- check if some other port can dock here
				// reset dock information
			gangster->parentLinked = -1;
			cditr->satisfied = false;
			cditr->lastKey = -1;
			cditr->boundGangster = NULL;
			cditr->boundDock = -1;
		}

			// retire this candidate (goes back into offer pool)
		gangster->retire( );
	}

		// undo dock information but leave gangster for reuse
	docks[dockNum].boundDock = -1;
	docks[dockNum].satisfied = false;
	docks[dockNum].lastKey = -1;

		// failed
	return( false );
}
