#include <iostream.h>
#include <iomanip.h>
#include "gen_lib.h"
#include "constants2.h"
#include <string.h>
#include <fstream.h>
#include <time.h>


void MakeLongLine(char ch1,
                  char ch2)
{
  int ch_count;

  cout << setw(6) << " ";
  for (ch_count=7; ch_count<124; ch_count++)
    if (ch_count%2)
      cout << ch1;
    else
      cout << ch2;
  cout << endl;
}


void MakeLine(char ch1,
              char ch2)
{
  int ch_count;

  for (ch_count=0; ch_count<79; ch_count++)
    if (ch_count%2)
      cout << ch1;
    else
      cout << ch2;
  cout << endl;
}


int OTStrLen(const char* s_ptr,
	     int         max)
{
  char* ptr;
  int   count;

  ptr = (char*) s_ptr;
  count = 0;
  while ((count < max) && (*ptr != '\0'))
    {
      count++;
      ptr++;
    }
  return count;
}


void PrintTime(ofstream& outfile)
{
  time_t current_time;
  char*  time_ptr;
  char   print_time[26];

  current_time = time(NULL);
  time_ptr = ctime(&current_time);
  if (time_ptr != NULL)
    {
      strncpy(print_time, time_ptr, 26);
      print_time[19] = '\0';
      outfile << print_time+4;
    }
  else
    outfile << "** CANNOT ACCESS TIME **";
}


extern "C" {
SetSyscalls() {}
}
