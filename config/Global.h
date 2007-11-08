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

/*************
**
** Global macros
**
*************/
#define YES	1
#define NO	0

/* SimpleConcat - concatenates two strings.  */
#ifndef SimpleConcat
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp) || defined(__ANSI_CPP__)
#define SimpleConcat(a,b)a##b
#else
#define SimpleConcat(a,b)a/**/b
#endif
#endif /* SimpleConcat */

/* The actual Concat rule which is robust to preprocessor semantics */
#ifndef Concat
#define Concat(a,b)SimpleConcat(a,b)
#endif /* Concat */

/* SimpleConcat3 - concatenates three strings.  */
#ifndef Concat3
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp) || defined(__ANSI_CPP__)
#define SimpleConcat3(a,b,c)a##b##c
#else
#define SimpleConcat3(a,b,c)a/**/b/**/c
#endif
#endif /* SimpleConcat3 */

/* The actual Concat3 rule which is robust to preprocessor semantics */
#ifndef Concat3
#define Concat3(a,b,c)SimpleConcat3(a,b,c)
#endif /* Concat3 */

/* SimpleConcat4 - concatenates four strings.  */
#ifndef SimpleConcat4
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp) || defined(__ANSI_CPP__)
#define SimpleConcat4(a,b,c,d)a##b##c##d
#else
#define SimpleConcat4(a,b,c,d)a/**/b/**/c/**/d
#endif
#endif /* SimpleConcat4 */

/* The actual Concat4 rule which is robust to preprocessor semantics */
#ifndef Concat4
#define Concat4(a,b,c,d)SimpleConcat4(a,b,c,d)
#endif /* Concat4 */

/* SimpleConcat5 - concatenates five strings.  */
#ifndef SimpleConcat5
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp) || defined(__ANSI_CPP__)
#define SimpleConcat5(a,b,c,d,e)a##b##c##d##e
#else
#define SimpleConcat5(a,b,c,d,e)a/**/b/**/c/**/d/**/e
#endif
#endif /* SimpleConcat5 */

/* The actual Concat5 rule which is robust to preprocessor semantics */
#ifndef Concat5
#define Concat5(a,b,c,d,e)SimpleConcat5(a,b,c,d,e)
#endif /* Concat5 */

/* SimpleConcat6 - concatenates six strings.  */
#ifndef SimpleConcat6
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp) || defined(__ANSI_CPP__)
#define SimpleConcat6(a,b,c,d,e,f)a##b##c##d##e##f
#else
#define SimpleConcat6(a,b,c,d,e,f)a/**/b/**/c/**/d/**/e/**/f
#endif
#endif /* SimpleConcat6 */

/* The actual Concat6 rule which is robust to preprocessor semantics */
#ifndef Concat6
#define Concat6(a,b,c,d,e,f)SimpleConcat6(a,b,c,d,e,f)
#endif /* Concat6 */

/* A simple rule to "string-ify" the given argument */
#define str(s) #s


/***************************************************************************
** Undefine any symbols the compiler might have defined for us since
** they may trip us up later.
***************************************************************************/
#undef sun
#undef sparc
#undef i386		
#undef i686
#undef hp9000s800
#undef __alpha
#undef sgi
#undef alpha
#undef hpux
#undef linux
#undef mips
#undef aix
#undef ia64
