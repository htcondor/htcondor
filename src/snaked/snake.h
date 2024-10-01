#ifndef   _CONDOR_SNAKE_H
#define   _CONDOR_SNAKE_H

// #include "condor_common.h"
// #include "condor_daemon_core.h"
// #include "snake.h"

class Snake : public Service {
    public:
        Snake( const std::string & snake_path ) : path(snake_path) { }

        virtual ~Snake();
        Snake & operator = (const Snake & other)    = delete;
        Snake(const Snake & other)                  = delete;
        Snake(Snake && other)                       = default;

        bool init();

        int CallPythonCommandHandler(const char * which_python_function, int command, Stream * sock);
        int CallPythonTimerHandler(const char * which_python_function);

    protected:
        std::string path;
};

#endif /* _CONDOR_SNAKE_H */
