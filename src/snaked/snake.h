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

        // For the Call() series of functions, the implementation adopts
        // the convention that the API functions are BumpyCaps, but the
        // internal coroutine implementations are snake_case.

        // These functions are intended to be called from lambdas which
        // curry the parameters on the first line and pass the subsequent
        // parameters through (from their own parameters).
        int CallCommandHandler(
            const char * which_python_function,
            int command, Stream * sock
        );

        // These functions are intended to be called from lambdas which
        // curry all of their parameters.
        int CallUpdateDaemonAd(const std::string & genus);
        void CallPollingHandler(const char * which_python_function, int which_signal);

    protected:
        std::string path;
};

#endif /* _CONDOR_SNAKE_H */
