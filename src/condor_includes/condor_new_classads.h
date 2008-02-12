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


#ifndef __CONDOR_NEW_CLASSADS_H
#define __CONDOR_NEW_CLASSADS_H

#include "condor_classad.h"

class DataSource;

class NewClassAdParser
{
 public:
    /** Constructor. Creates an object that can be used to parse ClassAds 
	 *  in the New ClassAd format and produce Old ClassAd objects.
     *  Not all aspects of the New ClassAd language are handled.
     *  See the comment in new_classads.C for _ParseClassAd().
	 */
	NewClassAdParser();

    /** Destructor. */
	~NewClassAdParser();

	/** Parses a string in New ClassAds format into an Old ClassAd
	 *  object. Only the first ClassAd in the buffer is parsed.
     *  Not all aspects of the New ClassAd language are handled.
     *  See the comment in new_classads.C for _ParseClassAd().
	 * @param buffer The string containing the new-style ClassAd.
	 * @return A pointer to a freshly-allocated ClassAd, which must be
	 *        deleted by the caller.
	 */
	ClassAd *ParseClassAd(const char *buffer);

	/** Parses a string in New ClassAds format into an Old ClassAd
     *  object.  Parsing begins at place-th * character of the string,
     *  and after a single ClassAd is parsed, place * is updated to
     *  indicate where parsing should resume for the next ClassAd.
     *  Not all aspects of the New ClassAd language are handled.
     *  See the comment in new_classads.C for _ParseClassAd().
	 * @param buffer The string containing the new-style ClassAd.
	 * @param place  A pointer to an integer indicating where to start. This is
	 *        updated upon return. 
	 * @return A pointer to a freshly-allocated ClassAd, which must be
	 *        deleted by the caller
	 */
	ClassAd *ParseClassAds(const char *buffer, int *place);

	/** Parses a file in New ClassAd format into an Old ClassAd object.
	 *  Only the first ClassAd in the buffer is parsed.
     *  Not all aspects of the New ClassAd language are handled.
     *  See the comment in new_classads.C for _ParseClassAd().
	 * @param buffer The string containing the new-style ClassAd.
	 * @return A pointer to a freshly-allocated ClassAd, which must be
	 *        deleted by the caller.
	 */
	ClassAd *ParseClassAd(FILE *file);

 private:
	ClassAd *_ParseClassAd(DataSource &source);
};

class NewClassAdUnparser
{
 public:
    /** Constructor. Creates an object that can be used to create New
	 * ClassAds representations of ClassAds */
	NewClassAdUnparser();

    /** Destructor.  */
	~NewClassAdUnparser();

	/** Query to find if the unparsed ad will use compact spacing. Compact
	 * spacing places an entire ClassAd on one line, while non-compact places
	 * each attribute on a separate line. 
	 * THIS FUNCTIONALITY IS UNIMPLEMENTED! COMPACT SPACING IS ALWAYS USED!
	 * @return true if using compact spacing. */
	bool GetUseCompactSpacing(void);

	/** Set compact spacing. Compact spacing 
	 * places an entire ClassAd on one line, while non-compact places
	 * each attribute on a separate line. 
	 * THIS FUNCTIONALITY IS UNIMPLEMENTED! COMPACT SPACING IS ALWAYS USED!
	 * @param use_compact_spacing A boolean indicating if compact spacing is
	 *       desired. */
	void SetUseCompactSpacing(bool use_compact_spacing);

	/** Query to find out if we will output the Type attribute.
	 *  @return true if we do, false if not. */
	bool GetOutputType(void);

	/** Set whether or not we output the Type attribute */
	void SetOutputType(bool output_type);

	/** Query to find out if we will output the TargetType attribute.
	 *  @return true if we do, false if not. */
	bool GetOuputTargetType(void);

	/** Set whether or not we output the Target attribute */
	void SetOutputTargetType(bool output_target_type);

	/** Get the string representation of an old ClassAd object in New
	 * ClassAds format. Note that this obeys the previously set
	 * parameters for compact spacing and compact names.
	 * @param classsad The ClassAd to represent in new ClassAds format.
	 * @param buffer The string to append the ClassAd to.
	 * @see SetUseCompactNames().
	 * @see SetUseCompactSpacing(). */
	bool Unparse(ClassAd *classad, MyString &buffer);

	/** Convert the string representation of an old ClassAd value to
	 *  the new ClassAd format.
	 * @param old_value The old ClassAd value to convert.
	 * @param new_value_buffer The string to append the new value to.
	 * @param err_msg Buffer for error messages (NULL if none). */
	bool OldValueToNewValue(char const *old_value,MyString &new_value_buffer,MyString *err_msg);

 private:
	bool _use_compact_spacing;
	bool _output_type;
	bool _output_target_type;
};

#endif /* __CONDOR_NEW_CLASSADS_H */
