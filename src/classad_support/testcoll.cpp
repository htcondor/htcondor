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


#include <ctype.h>
#include "classad/classad_distribution.h"

enum Commands {
	_NO_CMD_,

		// xaction control
	OPEN_XACTION,
	COMMIT_XACTION,
	ABORT_XACTION,
	SET_XACTION,

		// classad control
	ADD_CLASSAD,
	UPDATE_CLASSAD,
	MODIFY_CLASSAD,
	REMOVE_CLASSAD,

		// view control
	CREATE_SUBVIEW,
	CREATE_PARVIEW,
	DELETE_VIEW,

		// display
	VIEW_CONTENT,
	SHOW_CLASSAD,

		// misc
	SACRED,	// sacred attribute names
	HELP,
	QUIT,
	
	_LAST_COMMAND_
};


char CommandWords[][32] = {
	"",

	"open_xaction",
	"commit_xaction",
	"abort_xaction",
	"set_xaction",

	"add_classad",
	"update_classad",
	"modify_classad",
	"remove_classad",

	"subview",
	"parview",
	"delete_view",

	"view_content",
	"show_all_views",
	"get_classad",

	"sacred",
	"help",
	"quit",

	""
};


void help( void );
int  findCommand( char * );


classad::ClassAdCollectionServer collection;

int main( void )
{
	char			cmdString[32], buffer1[2048], buffer2[2048];
	std::string			currentXaction;
	int				command;
	Source			src;
	link;			snk;
	FormatOptions	fo;
	
	snk.SetSink( stdout );
	snk.SetTerminalChar( '\n' );
	snk.SetFormatOptions( &fo );

	if( !collection.InitializeFromLog( "log.log" ) ) {
		fprintf( stderr, "Failed to initialize from log; error=%d: %s\n", 
			classad::CondorErrno, classad::CondorErrMsg.c_str( ) );
		exit( 1 );
	}

	printf( "'h' for help" );
	while( 1 ) {
		classad::CondorErrMsg = "";
		classad::CondorErrno = classad::ERR_OK;
		printf("\n%s: ",currentXaction.empty()?"(none)":currentXaction.c_str());
		scanf( "%s", cmdString );
		command = findCommand( cmdString );

		switch( command ) {
				// xaction control
			case OPEN_XACTION: {
				scanf( "%s", buffer1 );
				if( !collection.OpenTransaction( buffer1 ) ) {
					fprintf(stderr,"Failed to open transaction: %s\n",buffer1 );
					fprintf( stderr, "%s\n", classad::CondorErrMsg.c_str( ) );
					classad::CondorErrMsg = "";
					continue;
				} else {
					currentXaction = buffer1;
				}
				break;
			}
				
			case COMMIT_XACTION: {
				int	outcome;
				if(!collection.CloseTransaction(currentXaction,true,outcome)||
						outcome == classad::ClassAdCollectionInterface::XACTION_ABORTED ){
					fprintf( stderr, "Failed to commit transaction: %s\n",
						currentXaction.c_str( ) );
					fprintf( stderr, "%s\n", classad::CondorErrMsg.c_str( ) );
					classad::CondorErrMsg = "";
				}
				currentXaction = "";
				break;
			}

			case ABORT_XACTION: {
				int outcome;
				if(!collection.CloseTransaction( currentXaction,false,outcome)||
						outcome != classad::ClassAdCollectionInterface::XACTION_ABORTED ){
					fprintf( stderr, "Failed to abort transaction: %s\n",
						currentXaction.c_str( ) );
					fprintf( stderr, "%s\n", classad::CondorErrMsg.c_str( ) );
					classad::CondorErrMsg = "";
				}
				currentXaction = "";
				break;
			}
			
			case SET_XACTION: {
				std::string	s;
				scanf( "%s", buffer1 );
				if( *buffer1 == '-' ) {
					currentXaction = "";
					break;
				}
				s = buffer1;
				if( collection.SetCurrentTransaction( s ) ) {
					currentXaction = s;
				} else {
					fprintf( stderr, "Failed to set transaction: %s\n",
						classad::CondorErrMsg.c_str( ) );
				}
				break;
			}

				// classad control
			case ADD_CLASSAD: 
			case UPDATE_CLASSAD:
			case MODIFY_CLASSAD: {
				classad::ClassAd	*ad=NULL;

				if( scanf( "%s", buffer1 ) != 1 ) {
					fprintf( stderr, "Error reading key\n" );
					fgets( buffer1, 2048, stdin );
					continue;
				}
				scanf( "%s", buffer2 );
				src.SetSource( buffer2 );
				if( !src.ParseClassAd( ad ) ) {
					fprintf( stderr, "Failed to parse classad: %s\n", buffer2 );
					break;
				}

				if( command == ADD_CLASSAD ) {
					if( !collection.AddClassAd(buffer1,ad)){
						fprintf( stderr, "Failed to add classad %s: %s\n", 
							buffer1, buffer2 );
						fprintf( stderr, "%s\n", classad::CondorErrMsg.c_str( ) );
						classad::CondorErrMsg = "";
						break;
					}
				} else if( command == UPDATE_CLASSAD ) {
					if(!collection.UpdateClassAd(buffer1, ad)){
						fprintf( stderr, "Failed to update classad %s: %s\n", 
							buffer1, buffer2 );
						fprintf( stderr, "%s\n", classad::CondorErrMsg.c_str( ) );
						classad::CondorErrMsg = "";
						break;
					}
				} else if( command == MODIFY_CLASSAD ) {
					if( !collection.ModifyClassAd(buffer1, ad)){
						fprintf( stderr, "Failed to modify classad %s: %s\n", 
							buffer1, buffer2 );
						fprintf( stderr, "%s\n", CondorErrMsg.c_str( ) );
						CondorErrMsg = "";
						break;
					}
				} 
				break;
			}

			case REMOVE_CLASSAD: {
				scanf( "%s", buffer1 );
				if( !collection.RemoveClassAd( buffer1 ) ) {
					fprintf( stderr, "Failed to remove classad %s\n", buffer1 );
					fprintf( stderr, "%s\n", CondorErrMsg.c_str( ) );
					CondorErrMsg = "";
				}
				break;
			}


				// display
			case VIEW_CONTENT: {
				scanf( "%s", buffer1 );
				if( !collection.DisplayView( buffer1, stdout ) ) {
					fprintf( stderr, "Failed to display view %s\n", buffer1 );
				}
				break;
			}

				// view control
			case CREATE_SUBVIEW: {
				char	constraint[2048], rank[2048], partitionExprs[2048];
				scanf( "%s %s %s %s %s", buffer1, buffer2, constraint, rank, 
					partitionExprs );
				if( !collection.CreateSubView( buffer1, buffer2, constraint, 
						rank, partitionExprs ) ) {
					fprintf( stderr, "Failed to create subview with:\n"
						"ViewName=%s, ParentViewName=%s, Constraint=%s, "
						"Rank=%s, PartitionExprs=%s\n", buffer1, buffer2, 
						constraint, rank, partitionExprs );
					fprintf( stderr, "%s\n", CondorErrMsg.c_str( ) );
					CondorErrMsg = "";
				}
				break;
			}
				
			case CREATE_PARVIEW: {
				ClassAd	*ad=NULL;
				char constraint[2048],rank[2048],partitionExprs[2048],rep[2048];

				scanf( "%s %s %s %s %s %s", buffer1, buffer2, constraint, rank, 
					partitionExprs, rep );

				if( !src.SetSource( rep ) || !src.ParseClassAd( ad ) ) {
					fprintf( stderr, "Failed to parse classad: %s\n", rep );
					break;
				}

				if( !collection.CreatePartition( buffer1, buffer2, constraint, 
						rank, partitionExprs, ad ) ) {
					fprintf( stderr, "Failed to create partition with:\n"
						"ViewName=%s, ParentViewName=%s, Constraint=%s, "
						"Rank=%s, PartitionExprs=%s, Rep=%s\n",buffer1,buffer2, 
						constraint, rank, partitionExprs, rep );
					fprintf( stderr, "%s\n", CondorErrMsg.c_str( ) );
					CondorErrMsg = "";
				}
				break;
			}

			case DELETE_VIEW: {
				scanf( "%s", buffer1 );
				if( !collection.DeleteView( buffer1 ) ) {
					fprintf( stderr, "Failed to delete view: %s\n" , buffer1 );
					fprintf( stderr, "%s\n", CondorErrMsg.c_str( ) );
					CondorErrMsg = "";
				}
				break;
			}

			case SHOW_CLASSAD: {
				ClassAd	*ad;
				scanf( "%s", buffer1 );
				if( !( ad = collection.GetClassAd( buffer1 ) ) ) {
					fprintf( stderr, "Failed to get classad %s\n", buffer1 );
					fprintf( stderr, "%s\n", CondorErrMsg.c_str( ) );
					CondorErrMsg = "";
					break;
				}
				ad->ToSink( snk );
				snk.FlushSink( );
				break;
			}

				// misc
			case SACRED:
			case HELP:
				help( );
				break;

			case QUIT:
				printf( "Done\n" );
				exit( 0 );

			default:
				break;
		}
		fgets( buffer1, 2048, stdin );
	}	
}

void
help( )
{
	printf( "\nCommands are:\n" );
	printf( "open_xaction <xtn-name>\n\tOpen new transaction; make it "
		"current\n\n");
	printf( "commit_xaction\n\tCommit current transaction\n\n" );
	printf( "abort_xaction\n\tAbort current transaction\n\n" );
	printf( "set_xaction <xtn-name>\n\t Make transaction <xtn-name> "
		"current\n\n" );

	printf( "add_classad <key><ad>\n\tAdd a new classad to the collection\n\n");
	printf( "update_classad <key><ad>\n\tUpdate classad <key>\n\n" );
	printf( "modify_classad <key><ad>\n\tModify classad <key>\n\n" );
	printf( "remove_classad <key>\n\tRemove classad from the collection\n\n" );

	printf( "subview <viewname><parentviewname><constraint><rank>"
				"<partitionExprs>\n\tCreate a subview of <parentview>\n\n" );
	printf( "parview <viewname><parentviewname><constraint><rank>"
				"<partitionExprs><rep>\n\tCreate a partitioned view of "
				"<parentview>\n\n");
	printf( "delete_view <viewname>\n\tDelete view <viewname>\n\n" );

	printf("view_content <viewname>\n\tDisplay content of view <viewname>\n\n");
	printf( "get_classad <key>\n\tShow classad <key>\n\n" );

	printf( "sacred\n\tShow names of 'sacred' attributes\n\n" );
	printf( "help\n\tThis screen\n\n" );
	printf( "quit\n\tQuit from the program\n\n" );
}


int
findCommand( char *cmdStr )
{
    int cmd = 0, len = strlen( cmdStr );

    for( int i = 0 ; i < len ; i++ ) {
        cmdStr[i] = tolower( cmdStr[i] );
    }

    for( int i = _NO_CMD_+1 ; i < _LAST_COMMAND_ ; i++ ) {
        if( strncmp( cmdStr, CommandWords[i], len ) == 0 ) {
            if( cmd == 0 ) {
                cmd = i;
            } else if( cmd > 0 ) {
                cmd = -1;
            }
        }
    }

    if( cmd > 0 ) return cmd;

    if( cmd == 0 ) {
        printf( "\nUnknown command: %s\n", cmdStr );
        return -1;
    }

    printf( "\nAmbiguous command %s; matches\n", cmdStr );
    for( int i = _NO_CMD_+1 ; i < _LAST_COMMAND_ ; i++ ) {
        if( strncmp( cmdStr, CommandWords[i], len ) == 0 ) {
            printf( "\t%s\n", CommandWords[i] );
        }
    }
    return -1;
}
