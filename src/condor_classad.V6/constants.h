#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

enum NumberFactor 
{ 
	NO_FACTOR 	= 1,
	K_FACTOR	= 1024,				// kilo
	M_FACTOR	= 1024*1024,		// mega
	G_FACTOR	= 1024*1024*1024	// giga
};

class Constant : public ExprTree 
{
  	public:
		// ctor/dtor
    	Constant ();
    	~Constant ();

    	// override methods
    	virtual ExprTree *copy (void);
		virtual bool toSink (Sink &);

    	// specific methods
    	void setIntegerValue    (int, NumberFactor = NO_FACTOR);
    	void setRealValue       (double, NumberFactor = NO_FACTOR);
    	void adoptStringValue   (char *);
    	void setStringValue     (char *);
    	void setUndefinedValue  (void);
    	void setErrorValue      (void);

  	private:
 		virtual void    _evaluate (EvalState &, Value &);

		// constant specific information
    	Value   		value;
		NumberFactor	factor;
};

#endif//__CONSTANTS_H__
