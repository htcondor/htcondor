#include <iostream.h>
#include <signal.h>


extern "C" int pause();
extern "C" int sigaction();


typedef void (*SIG_HANDLER)();


void sighandler(int sig_num);


int main(void)
{
  struct sigaction usr1, usr2, temp;

  usr1.sa_handler = (SIG_HANDLER) sighandler;
  sigemptyset(&usr1.sa_mask);
  usr1.sa_flags = 0;
  usr2.sa_handler = (SIG_HANDLER) sighandler;
  sigemptyset(&usr2.sa_mask);
  usr2.sa_flags = 0;
  if (sigaction(SIGUSR1, &usr1, &temp) < 0)
    {
      cerr << "sigaction() error" <<endl;
      exit(-1);
    }
  if (sigaction(SIGUSR2, &usr2, &temp) < 0)
    {
      cerr << "sigaction() error" <<endl;
      exit(-1);
    }
  while (1)
    pause();
}


void sighandler(int sig_num)
{
  if (sig_num == SIGUSR1)
    cout << "SIGUSR1 signal received" << endl;
  else if (sig_num == SIGUSR2)
    cout << "SIGUSR2 signal received" << endl;
  else
    cout << "Bogus crap received" << endl;
}

