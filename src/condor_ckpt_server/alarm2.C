#include <unistd.h>
#include "alarm2.h"
#include "signal2.h"


void Alarm::SetAlarm(int socket_desc,
		     int sec)
{
  BlockSignal(SIGALRM);
  alarm_sd = socket_desc;
  alarm(sec);
  UnblockSignal(SIGALRM);
}


void Alarm::ResetAlarm()
{
  BlockSignal(SIGALRM);
  alarm(0);
  UnblockSignal(SIGALRM);
}


void Alarm::Expired()
{
  BlockSignal(SIGALRM);
  if (alarm_sd != 0)
    close(alarm_sd);
  UnblockSignal(SIGALRM);
}
