/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

// File: Helper.cpp
// Author: Francesco Giacomini, INFN <Francesco.Giacomini@cnaf.infn.it>

// $Id: helper.C,v 1.3 2004-05-21 21:19:53 roy Exp $

#include "condor_common.h"
#include "helper.h"

#if defined(BUILD_HELPER)
#include <fstream>
#include <sstream>
#include <cstdlib>		// for system()
#include <string.h>		// for strlen()
//#include "condor_debug.h"
#include "condor_config.h"

class Helper::HelperImpl
{
public:
  std::string resolve(std::string const& input_file);
};

Helper::Helper()
  : m_impl(new HelperImpl)
{
  ASSERT( m_impl != 0 );
}

Helper::~Helper()
{
  delete m_impl;
}

std::string
Helper::resolve(std::string const& input_file) const
{
  return m_impl->resolve(input_file);
}

std::string
Helper::HelperImpl::resolve(std::string const& input_file)
{
  ASSERT( ! input_file.empty() );
  {
    std::string valid_chars("abcdefghijklmnopqrstuvwxyz"
			    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			    "0123456789_-.");
#ifdef WIN32
    valid_chars += "\:";
#else
    valid_chars += "/";
#endif
    std::string::size_type s = input_file.find_first_not_of(valid_chars.c_str());
				     
    bool input_file_name_is_valid = s == std::string::npos;
    ASSERT( input_file_name_is_valid );
  }
  {
    std::ifstream input_file_exists(input_file.c_str());
    ASSERT( input_file_exists );
  }
    
  std::string output_file;

  for (int i = 1; output_file.empty(); ++i) {
    std::ostringstream s;
    s << i;
    std::string tmp = input_file;
	tmp.append("-");
	tmp.append(s.str());
	tmp.append(".help");
    std::ifstream is(tmp.c_str());
    if (! is) {			// file does not exist: good!
      output_file = tmp;
    }
  }

  std::string helper_command("/bin/cp"); // default
  char const* p = param("DAGMAN_HELPER_COMMAND");
  if (p && strlen(p) != 0) {
    helper_command = p;
  }
  helper_command.append(" ");
  helper_command.append(input_file);
  helper_command.append(" ");
  helper_command.append(output_file);

  std::cout << helper_command << '\n';

  int status = system(helper_command.c_str());
  ASSERT( status == 0 );

  return output_file;
}

#endif //BUILD_HELPER
