class Alarm
{
  private:
    int alarm_sd;
    
  public:
    Alarm() { alarm_sd = 0; }
    void SetAlarm(int socket_desc,
		  int sec);
    void ResetAlarm();
    void Expired();
};



