/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
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

#ifndef FORMAT_OPTS_H
#define FORMAT_OPTS_H

BEGIN_NAMESPACE( classad )

/** Options to set formats of external representations */
class FormatOptions
{
    public:
        /// Constructor
        FormatOptions( );

        /// Destructor
        ~FormatOptions( );

        /** Sets the option for indented classads.
            @param i true if indented classads are required; false otherwise.
            @param len the indentation length (ignored if i is false)
        */
        void SetClassAdIndentation( bool i=true, int len=4 );

        /** Gets the current classad indentation options.
            @param i true if indentation is set, false otherwise
            @param len The indentation length if i is true, undefined otherwise
        */
        void GetClassAdIndentation( bool &i, int &len );

        /** Sets the option for indented lists.
            @param i true if indented lists are required; false otherwise.
            @param len the indentation length
        */
        void SetListIndentation( bool i=true, int len=4 );

        /** Gets the current list indentation options.
            @param i true if indentation is set, false otherwise
            @param len The indentation length if i is true, undefined otherwise
        */
        void GetListIndentation( bool &i, int &len );

        /** Sets option if string literals should be wrapped with quotes.  This
            option is useful for display purposes.  Note that an expression
            that is unparsed to a sink with this option off cannot (in general)
            be parsed back into an expression.
            @param w True if quotes are required, false otherwise.
        */
        void SetWantQuotes( bool );

        /** Gets the current quote display option setting
            @return true if quotes are required, false otherwise
        */
        bool GetWantQuotes( );

        /** Sets option for display with minimal parentheses.
            @param m true if minimal parentheses are required, false otherwise.
        */
        void SetMinimalParentheses( bool );

        /** Gets current minimal parentheses option setting
            @return true if minimal parentheses will be displayed, false
                otherwise.
        */
        bool GetMinimalParentheses( );

        // unimplemented --- experimental
        void SetMarginWrap( int cols, int indentLen );
        void GetMarginWrap( int &cols, int &indentLen );
    private:
        friend class ExprTree;
        friend class ClassAd;
        friend class AttributeReference;
        friend class ExprList;
        friend class FunctionCall;
        friend class Operation;
        friend class Literal;
        friend class Value;

        void SetIndentLevel( int );
        int  GetIndentLevel( );

        void SetPrecedenceLevel( int );
        int  GetPrecedenceLevel( void );

        bool    indentClassAds;
        bool    indentLists;
        int     indentLevel;
        int     classadIndentLen;
        int     listIndentLen;
        bool    wantQuotes;
        bool    minimalParens;
        int     precedenceLevel;
        int     marginWrapCols;
        int     marginIndentLen;
};

END_NAMESPACE // classad

#endif//FORMAT_OPTS_H
