#ifndef __FILE_C
#define __FILE_C


#define min(a,b)	( (a < b)? a : b )

class Header
{
protected:
	int	activeUsers;
};

class Data : public Header
{
protected:
	char	userName[MaxUserNameSize];
	int	priority;
};


class File : private Data
{
private:
	char*  fileName;   // full name of disk file where upDown is stored
	Header header;
	Data*	data; 

public :
	File(const char* filename);		  // constructor
	~File(void);				  // destructor
	int  operator << (const UpDown & upDown); // returns Success or Error
	int  operator >> (UpDown & upDown);       // returns Success or Error
};

#endif __FILE_C

