#ifdef IN_CONDOR
#include "condor_common.h"
#include "condor_classad.h"
#include "collection.h"
#else
#include <ctype.h>
#include "classad_package.h"
#endif

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
	SHOW_ALL_VIEWS,
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
	"show_classad",

	"sacred",
	"help",
	"quit",

	""
};


void help( void );
int  findCommand( char * );


ClassAdCollectionServer collection( "" );

int main( void )
{
	char			cmdString[32], buffer1[2048], buffer2[2048];
	string			currentXaction;
	int				command;
	Source			src;
	Sink			snk;
	FormatOptions	fo;
	
	snk.SetSink( stdout );
	snk.SetTerminalChar( '\n' );
	snk.SetFormatOptions( &fo );

	printf( "'h' for help" );
	while( 1 ) {
		printf("\n%s: ",currentXaction.empty()?"(none)":currentXaction.c_str());
		scanf( "%s", cmdString );
		command = findCommand( cmdString );

		switch( command ) {
				// xaction control
			case OPEN_XACTION: {
				scanf( "%s", buffer1 );
				if( !collection.OpenTransaction( buffer1 ) ) {
					fprintf(stderr,"Failed to open transaction: %s\n",buffer1 );
					continue;
				}
				currentXaction = buffer1;
				break;
			}
				
			case COMMIT_XACTION: {
				if( !collection.CloseTransaction( currentXaction, true ) ) {
					fprintf( stderr, "Failed to commit transaction: %s\n",
						currentXaction.c_str( ) );
				}
				currentXaction = "";
				break;
			}

			case ABORT_XACTION: {
				if( !collection.CloseTransaction( currentXaction, false ) ) {
					fprintf( stderr, "Failed to abort transaction: %s\n",
						currentXaction.c_str( ) );
				}
				currentXaction = "";
				break;
			}
			
			case SET_XACTION: {
				string	s;
				scanf( "%s", buffer1 );
				s = buffer1;
				break;
			}

				// classad control
			case ADD_CLASSAD: 
			case UPDATE_CLASSAD:
			case MODIFY_CLASSAD: {
				ClassAd	*ad=NULL;

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
					if( !collection.AddClassAd( currentXaction, buffer1, ad ) ){
						fprintf( stderr, "Failed to add classad %s: %s\n", 
							buffer1, buffer2 );
						if( ad ) delete ad;
						break;
					}
				} else if( command == UPDATE_CLASSAD ) {
					if( !collection.UpdateClassAd(currentXaction,buffer1,ad)){
						fprintf( stderr, "Failed to update classad %s: %s\n", 
							buffer1, buffer2 );
						if( ad ) delete ad;
						break;
					}
				} else if( command == MODIFY_CLASSAD ) {
					if( !collection.ModifyClassAd(currentXaction,buffer1,ad)){
						fprintf( stderr, "Failed to modify classad %s: %s\n", 
							buffer1, buffer2 );
						if( ad ) delete ad;
						break;
					}
				}
				break;
			}

			case REMOVE_CLASSAD: {
				scanf( "%s", buffer1 );
				if( !collection.RemoveClassAd( currentXaction, buffer1 ) ) {
					fprintf( stderr, "Failed to remove classad %s\n", buffer1 );
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
				if( !collection.CreateSubView( currentXaction, buffer1, buffer2,
						constraint, rank, partitionExprs ) ) {
					fprintf( stderr, "Failed to " );
				} else {
					fprintf( stderr, "Did " );
				}
				fprintf( stderr, "create subview with:\n"
						"ViewName=%s, ParentViewName=%s, Constraint=%s, "
						"Rank=%s, PartitionExprs=%s\n", buffer1, buffer2, 
						constraint, rank, partitionExprs );
				break;
			}
				
			case CREATE_PARVIEW: {
				ClassAd	*ad;
				char constraint[2048],rank[2048],partitionExprs[2048],rep[2048];

				scanf( "%s %s %s %s %s %s", buffer1, buffer2, constraint, rank, 
					partitionExprs, rep );

				if( !src.SetSource( rep ) || !src.ParseClassAd( ad ) ) {
					fprintf( stderr, "Failed to parse classad: %s\n", rep );
					break;
				}

				if( !collection.CreatePartition( currentXaction,buffer1,buffer2,
						constraint, rank, partitionExprs, ad ) ) {
					fprintf( stderr, "Failed to " );
				} else {
					fprintf( stderr, "Did " );
				}
				fprintf( stderr, "create partition with:\n"
						"ViewName=%s, ParentViewName=%s, Constraint=%s, "
						"Rank=%s, PartitionExprs=%s, Rep=%s\n",buffer1,buffer2, 
						constraint, rank, partitionExprs, rep );
				delete ad;
				break;
			}

			case DELETE_VIEW: {
				scanf( "%s", buffer1 );
				if( !collection.DeleteView( currentXaction, buffer1 ) ) {
					fprintf( stderr, "Failed to delete view: %s\n" , buffer1 );
				}
				break;
			}

			case SHOW_ALL_VIEWS: {
				printf( "Not implemented\n" );
				break;
			}

			case SHOW_CLASSAD: {
				ClassAd	*ad;
				scanf( "%s", buffer1 );
				if( !( ad = collection.GetClassAd( buffer1 ) ) ) {
					fprintf( stderr, "Failed to get classad %s\n", buffer1 );
					break;
				}
				ad->ToSink( snk );
				snk.FlushSink( );
				delete ad;
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
	printf( "use_xaction <xtn-name>\n\t Make transaction <xtn-name> "
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
	printf( "show_all_views\n\tShow all views\n\n" );
	printf( "show_classad <key>\n\tShow classad <key>\n\n" );

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
