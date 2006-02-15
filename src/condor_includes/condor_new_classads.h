/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __CONDOR_NEW_CLASSADS_H
#define __CONDOR_NEW_CLASSADS_H

#include "condor_classad.h"

class DataSource;

class NewClassAdParser
{
 public:
    /** Constructor. Creates an object that can be used to parse ClassAds */
	NewClassAdParser();

    /** Destructor. */
	~NewClassAdParser();

	/** Parses a New ClassAds ad. Only the first ClassAd in the buffer is 
	 *  parsed.
	 * @param buffer The string containing the new-style ClassAd.
	 * @return A pointer to a freshly-allocated ClassAd, which must be
	 *        deleted by the caller.
	 */
	ClassAd *ParseClassAd(const char *buffer);

	/** Parses a New ClassAds ad from a string. Parsing begins at place-th
	 * character of the string, and after a single ClassAd is parsed, place
	 * is updated to indicate where parsing should resume for the next ClassAd.
	 * @param buffer The string containing the new-style ClassAd.
	 * @param place  A pointer to an integer indicating where to start. This is
	 *        updated upon return. 
	 * @return A pointer to a freshly-allocated ClassAd, which must be
	 *        deleted by the caller
	 */
	ClassAd *ParseClassAds(const char *buffer, int *place);

	/** Parses a New ClassAds ad from a file. Only the first ClassAd in the 
	 * buffer is parsed.
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

	/** Format a ClassAd in New ClassAds format. Note that this obeys the
	 * previously set parameters for compact spacing and compact names. 
	 * @param classsad The ClassAd to represent in new ClassAds format.
	 * @param buffer The string to append the ClassAd to.
	 * @see SetUseCompactNames().
	 * @see SetUseCompactSpacing(). */
	bool Unparse(ClassAd *classad, MyString &buffer);

 private:
	bool _use_compact_spacing;
	bool _output_type;
	bool _output_target_type;
};

#endif /* __CONDOR_NEW_CLASSADS_H */
