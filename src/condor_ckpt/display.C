#include <stdio.h>
#include "image.h"

int
main( int argc, char *argv[] )
{
	Image my_image;

	my_image.Read( "Ckpt.13.13" );
	my_image.Display();
}
