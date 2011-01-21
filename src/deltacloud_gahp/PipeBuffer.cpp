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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "PipeBuffer.h"
#include "dcloudgahp_common.h"

PipeBuffer::PipeBuffer()
{
    pipe_end = -1;
}

std::string *
PipeBuffer::GetNextLine ()
{
    bool full_line;
    int i;
    char ra_buffer[PIPE_BUFFER_READAHEAD_SIZE];
    char c;
    ssize_t bytes;
    std::string buffer;
    std::string *result;
    bool last_char_was_escape;

    full_line = false;
    result = NULL;
    last_char_was_escape = false;

    while (!full_line) {
        memset(ra_buffer, 0, sizeof(ra_buffer));
        bytes = read(pipe_end, ra_buffer, sizeof(ra_buffer));
        if (bytes < 0) {
            dcloudprintf ("error reading from pipe %d\n", pipe_end);
            break;
        }
        if (bytes == 0) {
            dcloudprintf("EOF reached on pipe %d\n", pipe_end);
            break;
        }

        for (i = 0; i < bytes; i++) {
            c = ra_buffer[i];

            if (c == '\n' && !last_char_was_escape) {
                // We got a completed line!
                result = new std::string(buffer);
                full_line = true;
                break;
            }
            else if (c == '\r' && !last_char_was_escape) {
                // Ignore \r
            }
            else {
                buffer += c;

                if (c == '\\')
                    last_char_was_escape = !last_char_was_escape;
                else
                    last_char_was_escape = false;
            }
        }
    }

    return result;
}
