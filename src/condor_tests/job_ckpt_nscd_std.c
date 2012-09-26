/*******************************************************************************
 *
 * Copyright (C) 2012, Condor Team, Computer Sciences Department,
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
 ******************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

void ckpt_and_exit( void );

int main( int argc, char **argv )
{
    printf( "Starting up, my pid is %d\n",getpid() );

    if ( argc > 1 && argv[1][0] == 'Y' ) {
        printf("Doing getpwnam()\n");
        getpwnam("foobar");
    } else {
        printf("Skipping getpwnam()\n");
    }

    printf( "Checkpoint and exit...\n" );
    ckpt_and_exit();

    printf( "Returned from checkpoint and exit\n");
    printf( "Exiting\n" );
    return 0;
}
