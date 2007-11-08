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


#include <stdio.h>

/* Configuration file format:
 *
 * Each line of the configuration file is one of the following formats
 *
 *  1.	blank line	blank lines are ignored
 *  2.	# comment	comment lines are ignored
 *  3.	key		key without a value
 *  4.	key = value	whitespace is trimmed around key and value
 *  5.	key <n>		n characters after the <NL> are the value as is
 *	value
 *
 * A key can be any sequence of charaters except '=' and whitespace characters.
 * Whitespace is always trimmed from the beginning and end of the key.
 *
 * In form 4, whitespace is trimmed from the beginning and end of the value, so
 * the value cannot begin or end with whitespace, or contain a new-line.
 *
 * In form 5, the value is the next n characters, so the value can contain
 * arbitrary contents.  This is the only way to represent values, not
 * representable by form 4.  A new-line may be placed after the value and it
 * will be processed as a blank line.  This format is mainly intended to be
 * used by machine generated configurations.
 *
 *
 * Grammar for configuration file entries:
 *
 *	<NL>		is a new-line character
 *	<SP>		is a non-<NL> whitespace character
 *	<KEYCHAR>	is any non-<SP> and not '='
 *	<DIGIT>		is '0' .. '9'
 *	<VALUECHAR>	is any non-<NL>
 *	<ANY>		is any char
 *	'x'		literal character 'x'
 *
 *	*		0 or more occurances
 *	*?		0 or more occurances minimal match
 *	+		1 or more occurances
 *	{n}		n occurances
 *
 *  1.	<SP>* <NL>
 *  2.	<SP>* '#' <VALUECHAR>* <NL>
 *  3.  <SP>* <KEYCHAR>+ <SP>* <NL>
 *  4.  <SP>* <KEYCHAR>+ <SP>* '=' <SP>* <VALUECHAR>*? <SP>* <NL>
 *  5.  <SP>* <KEYCHAR>+ <SP>* '<' <SP>* <DIGIT>* <SP>* '>' <SP>* <NL> <ANY>{n}
 */

typedef struct config_file {
    int line_num;
    size_t bytes_read;
    size_t prev_value_extra_lines;
    const char *filename;
    FILE *F;
} config_file;

int open_config_stream(config_file *cf, const char *filename, FILE * F);
int open_config_file(config_file *cf, const char *filename);
int close_config_stream(config_file *cf);
int close_config_file(config_file *cf);
int next_cf_value(config_file *cf, char **key, char **value);
