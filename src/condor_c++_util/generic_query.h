#ifndef __GENERIC_QUERY_H__
#define __GENERIC_QUERY_H__

#include "condor_classad.h"
#include "list.h"
#include "simplelist.h"


// error values
enum 
{
	Q_OK,
	Q_INVALID_CATEGORY,
	Q_MEMORY_ERROR,
	Q_PARSE_ERROR,
};
	

class GenericQuery
{
  public:
	// ctor/dtor
	GenericQuery ();
	GenericQuery (const GenericQuery &);
	~GenericQuery ();

	// set number of categories
	int setNumIntegerCats (const int);
	int setNumStringCats (const int);
	int setNumFloatCats (const int);
	
	// add constraints
	int addInteger (const int, int);
	int addString (const int, char *);
	int addFloat (const int, float);
	int addCustom (char *);

	// clear constraints
	int clearInteger (const int);
	int clearString (const int);
	int clearFloat (const int);
	int clearCustom (void);

	// set keyword lists
	void setIntegerKwList (char **);
	void setStringKwList (char **);
	void setFloatKwList (char **);
	
	// make the query Attrlist (N.B. types *not* set)
	int makeQuery (ClassAd &);

	// overloaded operators
    // friend ostream &operator<< (ostream &, GenericQuery &);  // display
    // GenericQuery   &operator=  (GenericQuery &);             // assignment

  private:
	// to store the number of categories of each type
	int integerThreshold;
	int stringThreshold;
	int floatThreshold;

	// keyword lists
	char **integerKeywordList;
	char **stringKeywordList;
	char **floatKeywordList;

	// pointers to store the arrays of Lists neessary to store the constraints
	SimpleList<int>   *integerConstraints;
	SimpleList<float> *floatConstraints;
	List<char> 		  *stringConstraints;
	List<char> 		  customConstraints;

	// helper functions
	void clearQueryObject     (void);
    void clearStringCategory  (List<char> &);
    void clearIntegerCategory (SimpleList<int> &);
    void clearFloatCategory   (SimpleList<float> &);
    void copyQueryObject      (GenericQuery &);
    void copyStringCategory   (List<char> &, List<char> &);
    void copyIntegerCategory  (SimpleList<int> &, SimpleList<int> &);
    void copyFloatCategory    (SimpleList<float>&, SimpleList<float>&);
};

#endif







