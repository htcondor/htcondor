#include <fstream.h>
#include <stdlib.h>
#include <stdio.h>


const int MAX_FILES = 250;
const int MAX_OWNERS = 15;
const int MAX_HOSTS = 5;


extern "C" int random();


int main(void)
{
  ofstream outfile("testfilestat.dat");
  ifstream infile("/etc/hosts");
  char*    machine[MAX_HOSTS] = {
                                  "grumpy",
				  "wiley",
				  "picard",
				  "jackal",
				  "miss-grabople"
                                };
  char*    owner[MAX_OWNERS] = {
                                 "tsao",
				 "kirkd",
				 "tjung",
				 "jjason",
				 "livny",
				 "pruyne",
				 "litzkow",
				 "solomon",
				 "ramanath",
				 "ajitk",
				 "jordan",
				 "pimmel",
				 "goodman",
				 "yannis",
				 "parshore"
                               };
  int count;
  int r;

  if (infile.fail())
    {
      cerr << "ERROR: Cannot open /etc/hosts file" << endl;
      exit(-1);
    }
  if (outfile.fail())
    {
      cerr << "ERROR: Cannot open output file" << endl;
      exit(-1);
    }
  for (count=0; count<MAX_FILES; count++)
    {
      r = random()%MAX_HOSTS;
      outfile << machine[r] << ' ';
      r = random()%MAX_OWNERS;
      outfile << owner[r] << ' ';
      outfile << "testfile.";
      r = random()%3;
      outfile << r << endl;
    }
  infile.close();
}





