#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "image.h"


extern Image MyImage;

int		UnInitDataObj;
char	InitDataObj[] = "Buff";
void	func();

void whereis( char *msg, void *obj, Image *image );

int
main( int argc, char *argv[] )
{
	int	local;

	DUMP( "", InitDataObj, 0x%X );
	DUMP( "", func, 0x%X );

	MyImage.Save();
	whereis( "Uninitialized Data Object", &UnInitDataObj, &MyImage );
	whereis( "Initialized Data Object", InitDataObj, &MyImage );
	whereis( "Automatic Data Object", &local, &MyImage );
	whereis( "Function", func, &MyImage );
	MyImage.Display();
	if( MyImage.Write("Ckpt.13.13") < 0 ) {
		perror( "MyImage.Write()" );
		exit( 1 );
	}
}

void
whereis( char *msg, void *obj, Image *image )
{
	char	*seg_name;

	if( seg_name = image->FindSeg(obj) ) {
		printf( "%s is in segment \"%s\"\n", msg, seg_name );
	} else {
		printf( "%s - Not Found\n", msg );
	}
}

void
func()
{
}
