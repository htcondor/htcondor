/* scanner.h
   Interface definition for the scanner module of condor submit.
*/
#ifndef _SCANNER_H
#define _SCANNER_H

#define MAX_VAR_NAME 100

/* Lexeme abstract dataype: */

class token
{
 public:
  token() { str_val = NULL; le_type = NOT_KEYWORD; }
  ~token() { if(le_type == LX_STRING) delete str_val; }
  friend void scanner (char **, class token *);
  lexeme_type token_type ();
  int token_int ();
  char *token_str ();
  float token_float();

  union {
   int int_val;
   char *str_val;
   float float_val;
  };

 private: 
  lexeme_type le_type; 
};


#endif /* _SCANNER_H */
