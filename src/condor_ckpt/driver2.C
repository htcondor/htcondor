#define _POSIX_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "image.h"

extern Image MyImage;



int
main( int argc, char *argv[] )
{
	if( MyImage.Read("Ckpt.13.13") < 0 ) {
		perror( "MyImage.Read()" );
		exit( 1 );
	}

	MyImage.Display();
	MyImage.Restore();

	exit( 0 );
}
