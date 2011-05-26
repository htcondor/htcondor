#ifndef CFTP_SERVICE_H_
#define CFTP_SERVICE_H_

class Service
{
 public:
    Service( int client_handle );
    virtual ~Service();
    
    int get_handle();
    bool is_finished();

    virtual void run();
    virtual void step() = 0;
    
    

 protected:

    int _handle;
    int _finished;

    


};



#endif
