//******************************************************************************
//
// scanner.h
//
//******************************************************************************

#ifndef _SCANNER_H_
#define _SCANNER_H_

#define MAX_VAR_NAME 100

// Lexeme abstract dataype:

class Token
{
    public:

        Token() { strVal = NULL; type = NOT_KEYWORD; }
        ~Token() { if(type == LX_STRING) delete strVal; }

        LexemeType	Type () { return type; }
        int		Int ()  { return intVal; }
        char*		Str () { return strVal; }
        float		Float() { return floatVal; }
	int		Length() { return length; }

        friend void Scanner (char*&, class Token&);

        union {
            int		intVal;
            char*	strVal;
            float	floatVal;
        };

    private: 

        LexemeType	type; 
	int		length;
};


#endif
