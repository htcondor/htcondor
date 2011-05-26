#include "cftp_service.h"

Service::Service( int handle ) : 
    _handle(handle), 
    _finished(0)
{ 
}


Service::~Service()
{

}

int Service::get_handle()
{
    return _handle;
}


bool Service::is_finished()
{
    return _finished;
}


void Service::run()
{
    while( ! _finished )
        step();
}
