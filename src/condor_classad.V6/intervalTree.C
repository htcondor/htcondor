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
#include "intervalTree.h"
#include <set>

IntervalTree::
IntervalTree( )
{
	nodes = NULL;
}

IntervalTree::
~IntervalTree( )
{
	if( nodes ) delete [] nodes;
}

IntervalTree *IntervalTree::
MakeIntervalTree( const OneDimension& intervals )
{
	OneDimension::const_iterator	itr;
	set<double> 					endPoints;
	double							tmpL, tmpR;

	// 1.Sort the endpoints, and eliminate duplicates
	for( itr = intervals.begin( ); itr != intervals.end( ); itr++ ) {
		tmpL = itr->second.lower;
		endPoints.insert( tmpL );
		tmpR = itr->second.upper;
		endPoints.insert( tmpR );
	}
	unsigned int numPoints = endPoints.size( );	// number of distinct endpoints

	// 2. Make the primary structure
	int	depth, overflow, retain, size, frontStart, backStart;

		// 2a. figure out the depth of the primary structure
	depth = 1;
	while( numPoints >> depth ) depth++;

		// 2b. number of points that cannot be accomodated at the depth
	overflow = 2 * ( numPoints - unsigned( 1<<(depth-1) ) );
	retain = numPoints - overflow;

		// 2c. calculate the start indexes and the number of nodes in tree
	frontStart = ( 1 << depth ) - 1;
	backStart = frontStart - retain;
	size = frontStart + overflow;

		// 2d. create storage for the interval tree
	IntervalTree	*intTree = new IntervalTree;
	IntervalTreeNode *nodes = new IntervalTreeNode[ size ];
	intTree->size = size;
	intTree->nodes = nodes;

		// 2e. insert leaf values at level (depth+1)
	set<double>::iterator sitr = endPoints.begin( );
	for( int i = frontStart ; i < frontStart+overflow ; i++, sitr++ ) {
		nodes[i].max = nodes[i].min = nodes[i].nodeValue = *sitr;
	}

		// 2f. insert leaf values at level (depth)
	for( int i = backStart ; i < backStart+retain ; i++, sitr++ ) {
		nodes[i].max = nodes[i].min = nodes[i].nodeValue = *sitr;
	}	
	
		// 2g. build primary structure of internal nodes
	for( int i = backStart - 1 ; i >= 0 ; i-- ) {
		nodes[i].nodeValue = ( nodes[2*i+1].max + nodes[2*i+2].min ) / 2;
		tmpL = nodes[2*i+1].max; tmpR = nodes[2*i+2].max;
		nodes[i].max = tmpL > tmpR ? tmpL : tmpR;
		tmpL = nodes[2*i+1].min; tmpR = nodes[2*i+2].min;
		nodes[i].min = tmpL < tmpR ? tmpL : tmpR;
	}


	// 3. Insert intervals into secondary structure
	for( itr = intervals.begin( ); itr != intervals.end( ); itr++ ) {
		int index = 0; // start at root
		tmpL = itr->second.lower;
		tmpR = itr->second.upper;
		while( 1 ) {
			if( index > size ) {
				printf( "Fell out of the primary structure" );
				exit( 1 );
			}
			if( tmpR < nodes[index].nodeValue ) {
				index = index*2 + 1;
				continue;
			} 
			if( tmpL > nodes[index].nodeValue ) {
				index = index*2 + 2;
				continue;
			} 
			if( tmpL<=nodes[index].nodeValue && tmpR>=nodes[index].nodeValue ) {
				if( !InsertInLeftSecondary(nodes[index].LS,itr->first,tmpL,
						itr->second.openLower) ||
					!InsertInRightSecondary(nodes[index].RS,itr->first,tmpR,
						itr->second.openUpper) ){
					printf( "Failed to insert in secondary list" );
					exit( 1 );
				}
					// this may be a leaf node, so set up data associated with
					// being an active node.  LT and RT pointers are important
					// only for non-leaf nodes, and will be set up when 
					// establishing the tertiary structure.
				nodes[index].active = true;
				nodes[index].activeDesc = true;
				nodes[index].closestActive = index;
				break;
			}
		}
	}

	// 4. Setup tertiary structure (non-leaf nodes only)
	int indexL, indexR;
	for( int i = backStart - 1; i >= 0 ; i-- ) {
		indexL = 2*i + 1;
		indexR = 2*i + 2;
			// 4a. figure out if the node is active/inactive
		nodes[i].active = ( nodes[i].LS!=NULL || nodes[i].RS!=NULL ) ||
			( nodes[indexL].activeDesc && nodes[indexR].activeDesc );

			// 4b. if the node is active, set its LT and RT pointers
		if( nodes[i].active ) {
			nodes[i].LT = nodes[indexL].closestActive;
			nodes[i].RT = nodes[indexR].closestActive;
				// active nodes trivially have active descendants
			nodes[i].activeDesc = true;
			nodes[i].closestActive = i;
		} else {
			// 4c. if the node is inactive, at most one of its children has
			//	   active descendants
			nodes[i].activeDesc = nodes[indexL].activeDesc || 
				nodes[indexR].activeDesc;
			nodes[i].closestActive = nodes[indexL].activeDesc ?
				nodes[indexL].closestActive : nodes[indexR].closestActive;
		}
	}


	// done!
	return( intTree );
}


bool IntervalTree::
DeleteInterval( const int& key, const NumericInterval &interval )
{
	double	l, r;
	int		i;

	l = interval.lower;
	r = interval.upper;

		// traverse the active tree to find split point
	i = 0;
	while( i >= 0 && i <= size ) {
		if( nodes[i].nodeValue > r ) {
			i = nodes[i].LT;
			continue;
		}
		if( nodes[i].nodeValue < l ) {
			i = nodes[i].RT;
			continue;
		}
		if( l <= nodes[i].nodeValue && r >= nodes[i].nodeValue ) {
			break;
		}
	}
		// no such interval in tree
	if( i < 0 || i >= size ) {
		printf( "Error:  No such interval [%f, %f] in tree!\n", l, r );
		return( false );
	}

		// delete from secondary structure
	if( !DeleteFromSecondary( nodes[i].LS, key, l, true ) ||
			!DeleteFromSecondary( nodes[i].RS, key, r, false ) ) {
		printf( "Failed to delete from secondary\n" );
		return( false );
	}

		// work towards root and adjust tertiary structure
	int 	indexL, indexR;
	bool 	nowActive;
	while( 1 ) {
		indexL = 2*i+1; 
		indexR = 2*i+2;
		nowActive = ( nodes[i].LS!=NULL || nodes[i].RS!=NULL ) ||
				(indexL > 0 && indexL <= size && indexR > 0 && indexR <= size &&
	    		nodes[indexL].activeDesc && nodes[indexR].activeDesc );
		if( nodes[i].active == nowActive ) {
				// active status unchanged
			nodes[i].active = true;
			nodes[i].activeDesc = true;
			nodes[i].closestActive = i;
				// check why we're active ...
			if( nodes[i].LS!=NULL || nodes[i].RS!=NULL ) {
					// active because of stuff in the secondary structure; done
				return( true );
			}
				// active because of active descendants; update LS, RS
			nodes[i].LT = nodes[indexL].closestActive;
			nodes[i].RT = nodes[indexR].closestActive;

				// done
			return( true );
		} 
			// must have gone from active to inactive
		if( !nodes[i].active && nowActive ) {
			printf("Error:  Gone from inactive to active when deleting\n" );
			return( false );
		}
		nodes[i].active = false;
		nodes[i].LT = nodes[i].RT = -1;

			// check if the node has active descendants; at most one if any
		if( ( indexL > 0 && indexL <= size && nodes[indexL].activeDesc ) || 
				( indexR > 0 && indexR <= size && nodes[indexR].activeDesc ) ) {
			nodes[i].activeDesc = true;
			nodes[i].closestActive = nodes[indexL].activeDesc ?
				nodes[indexL].closestActive : nodes[indexR].closestActive;
		} else {
			nodes[i].activeDesc = false;
			nodes[i].closestActive = -1;
		}

			// reached root?
		if( i == 0 ) return( true );

		i = (i-1) / 2;
	}

	return( true );
}


bool IntervalTree::
InsertInLeftSecondary( Secondary *&list, const int &key, double val, bool open )
{
	Secondary *tmp, *sec = new Secondary;
	if( !sec ) return( false );
	sec->value 	= val;
	sec->key	= key;
	sec->open 	= open;
	
	if( list==NULL || list->value>val || (list->value==val && open) ) {
		sec->next = list;
		list = sec;
		return( true );
	}

	tmp = list;
	while( tmp->next && ( tmp->next->value<val || tmp->next->value==val && 
			tmp->next->open ) ) {
		tmp = tmp->next;
	}
	sec->next = tmp->next;
	tmp->next = sec;
	return( true );
}


bool IntervalTree::
InsertInRightSecondary(Secondary *&list, const int &key, double val, bool open)
{
	Secondary *tmp, *sec = new Secondary;
	if( !sec ) return( false );
	sec->value 	= val;
	sec->key	= key;
	sec->open 	= open;
	
	if( list==NULL || list->value<val || (list->value==val && !open) ) {
		sec->next = list;
		list = sec;
		return( true );
	}

	tmp = list;
	while( tmp->next && ( tmp->next->value>val || tmp->next->value==val && 
			!tmp->next->open ) ) {
		tmp = tmp->next;
	}
	sec->next = tmp->next;
	tmp->next = sec;
	return( true );
}


bool IntervalTree::
DeleteFromSecondary( Secondary *&list,const int &key,double val,bool less ) 
{
	Secondary *tmp, *sec = list;

	if( sec == NULL ) return( false );
	if( sec->value==val && sec->key==key ) {
		list = sec->next;
		delete sec;
		return( true );
	}

	tmp = list;
	sec = list->next;
	while( sec && ( less ? sec->value < val : sec->value > val ) ) {
		tmp = sec;
		sec = sec->next;
	}
	if( !sec ) return( false );

	while( sec && sec->value == val ) {
		if( sec->key == key ) {
			tmp->next = sec->next;
			delete sec;
			return( true );
		}
		tmp = sec;
		sec = sec->next;
	}

		// some error
	return( false );
}


bool IntervalTree::
WindowQuery( const NumericInterval& interval, KeySet& keys )
{
	double		l, r;
	bool		ol, or;
	int			i, v;
	int			index, offset;
	Secondary	*sec;

	keys.fill( 0 );
	l = interval.lower;
	r = interval.upper;
	ol = interval.openLower;
	or = interval.openUpper;

		// Phase 1:  find v such that l < val(v) < r
	i = 0;
	while( 1 ) {
		if( i < 0 || i >= size ) {
			return( true );
		} else if( nodes[i].nodeValue <= l ) {
			sec = nodes[i].RS;
			while(sec&&(sec->value>l || (sec->value==l && !sec->open && !ol))){
				index = sec->key / SUINT;
				offset = sec->key % SUINT;
				keys[index] = keys[index] | (1<<offset);
				sec = sec->next;
			}
			//i = 2*i + 2;	// search right subtree next
			i = nodes[i].RT;
		} else if( r <= nodes[i].nodeValue ) {
			sec = nodes[i].LS;
			while(sec&&(sec->value<r || (sec->value==r && !sec->open && !or))){
				index = sec->key / SUINT;
				offset = sec->key % SUINT;
				keys[index] = keys[index] | (1<<offset);
				sec = sec->next;
			}
			//i = 2*i + 1;	// search left subtree next
			i = nodes[i].LT;
		} else if( l < nodes[i].nodeValue && nodes[i].nodeValue < r ) {
			break;
		} else {
			printf( "Error:  failed to find split node\n" );
			exit( 1 );
		}
	}
	v = i;	// found v; report all intervals in secondary structure of v
	sec = nodes[i].LS;
	while( sec ) {
		index = sec->key / SUINT;
		offset = sec->key % SUINT;
		keys[index] = keys[index] | (1<<offset);
		sec = sec->next;
	}

		// Phase 2: start at v and locate l in the left subtree of v
	//i = 2*v + 1;
	i = nodes[v].LT;
	while( i >= 0 && i < size ) {
		if( l < nodes[i].nodeValue ) {
				// get all nodes in the secondary structure
			sec = nodes[i].RS;
			while( sec ) {
				index = sec->key / SUINT;
				offset = sec->key % SUINT;
				keys[index] = keys[index] | (1<<offset);
				sec = sec->next;
			}
			VisitActive( nodes[i].RT, keys );
			i = nodes[i].LT;
		} else {
			sec = nodes[i].RS;
			while(sec&&(sec->value>l || (sec->value==l && !sec->open && !ol))){
				index = sec->key / SUINT;
				offset = sec->key % SUINT;
				keys[index] = keys[index] | (1<<offset);
				sec = sec->next;
			}
			i = nodes[i].RT;
		}
	}

		// Phase 3: start at v and locate r in the right subtree of v
	//i = 2*i + 2;
	i = nodes[v].RT;
	while( i >= 0 && i < size ) {
		if( r > nodes[i].nodeValue ) {
				// get all nodes in the secondary structure
			sec = nodes[i].LS;
			while( sec ) {
				index = sec->key / SUINT;
				offset = sec->key % SUINT;
				keys[index] = keys[index] | (1<<offset);
				sec = sec->next;
			}
			VisitActive( nodes[i].LT, keys );
			i = nodes[i].RT;
		} else {
			sec = nodes[i].LS;
			while(sec&&(sec->value<r || (sec->value==r && !sec->open && !or))){
				index = sec->key / SUINT;
				offset = sec->key % SUINT;
				keys[index] = keys[index] | (1<<offset);
				sec = sec->next;
			}
			i = nodes[i].LT;
		}
	}

		// done
	return( true );
}


void IntervalTree::
VisitActive( int i, KeySet& keys )
{
	if( i==-1 || i>=size ) return;

		// get all intervals in this node
	Secondary 	*sec = nodes[i].LS;
	int			index, offset;
	while( sec ) {
		index = sec->key / SUINT;
		offset = sec->key % SUINT;
		keys[index] = keys[index] | (1<<offset);
		sec = sec->next;
	}

		// recurse on left and right tertiary structures
	VisitActive( nodes[i].LT, keys );
	VisitActive( nodes[i].RT, keys );
}


void IntervalTree::
Display( FILE* fp )
{
	if( size <= 0 || !nodes ) {
		fprintf( fp, "<empty>\n" );
		return;
	}

	int 		j = 1, k = 1;
	Secondary 	*sec;
	for( int i = 0 ; i < size; i++ ) {
		fprintf( fp, " [%g:", nodes[i].nodeValue );
		for( sec = nodes[i].LS; sec != NULL; sec = sec->next ) {
			fprintf( fp, "%g=", sec->value );
		}
		putc( ':', fp );
		for( sec = nodes[i].RS; sec != NULL; sec = sec->next ) {
			fprintf( fp, "%g=", sec->value );
		}
		fprintf( fp, "|%d,%d]  ", nodes[i].LT, nodes[i].RT );

		if( j == k ) {
			putc( '\n', fp );
			k = k << 1;
			j = 1;
		} else {
			j++;
		}
	}
	putc( '\n', fp );
}
