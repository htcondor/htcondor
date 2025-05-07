#ifndef   _CONDOR_CALL_BEFORE_EXIT
#define   _CONDOR_CALL_BEFORE_EXIT

// #include <function>

class CallBeforeExit {

    public:
        CallBeforeExit( std::function<void (void)> fn ) : f(fn) { }
        virtual ~CallBeforeExit() { f(); }

    protected:
        std::function<void (void)> f;

};

#endif /* _CONDOR_CALL_BEFORE_EXIT */
