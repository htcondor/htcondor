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
//******************************************************************************
//
// scanner.h
//
//******************************************************************************

#ifndef _SCANNER_H_
#define _SCANNER_H_

#define MAXVARNAME 256

#define USE_NEW_SCANNER

#include "condor_classad.h"

class Token
{
	public:

		Token();
		~Token();
		void		reset();
		union
		{
			int		intVal;
			float	floatVal;
		};
		//LexemeType	type; 
		int			length;	// error position in the string for the parser
#ifdef USE_NEW_SCANNER
		char        *strVal;
		int         strValLength;
#else
		char		strVal[ATTRLIST_MAX_EXPRESSION];
#endif
		friend	void	Scanner(char *&input, Token &token);

	private:

		int		isString;

};

#endif
