// This class has the same interface as the list class found in
// condor_c++_utils.  Unfortunately, the aforementioned class is written to
// work with pointers and references, and is thus not great for storing
// persistent information when the items are so small as to not warrant
// dynamic memory allocation.
//     -- Rajesh

#ifndef _INTLIST_H_
#define _INTLIST_H_

class IntList 
{
    public:
    // ctor, dtor
    IntList ();
    ~IntList ();

    // General
    int Append (int);
    int IsEmpty();

    // Scans
    void    Rewind();
    int     Current(int&);
    int     Next(int&);
    int     AtEnd();
    void    DeleteCurrent();

    // Debugging
    void    Display();

    private:
    enum {MAXIMUM_SIZE = 32};
    int  items[MAXIMUM_SIZE];
    int  size;
    int  current;
};

#endif // _INTLIST_H_
