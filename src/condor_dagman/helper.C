/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

// File: Helper.cpp
// Author: Francesco Giacomini, INFN <Francesco.Giacomini@cnaf.infn.it>

// $Id: helper.C,v 1.1.4.4 2003-01-18 19:30:56 epaulson Exp $

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
    std::string tmp = input_file + '-' + s.str() + ".help";
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
  helper_command += " " + input_file + " " + output_file;

  std::cout << helper_command << '\n';

  int status = system(helper_command.c_str());
  ASSERT( status == 0 );

  return output_file;
}

#endif //BUILD_HELPER
