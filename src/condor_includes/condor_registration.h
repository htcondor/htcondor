//
// registration.h :
//
//   registration of the types of ClassAd instances. 
//
//   Written by Wei Chen & Taxiao Wang, Fall 1995.
//

#ifndef _REGISTRATION_H
#define _REGISTRATION_H

class Registration
{ 
    public:

        Registration(int = 10);           // constructor.
        ~Registration();                  // destructor.
        int RegisterType(char *);         // it registers a (new) type. 

    private:

        char **regiTable;                 // registration table.
        int regiTableSize;                // size of the registration table. 
        int regiNumber;                   // number of registrations.
};

#endif
