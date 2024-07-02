#define COMMIT_TRANSACTION true
#define ABORT_TRANSACTION false

class QueueConnection {
  public:
    bool connect( const char * addr );
    bool connect( DCSchedd & schedd );

    bool abort();
    bool commit( std::string & message );

    ~QueueConnection();

  private:
    bool schedd_connect( DCSchedd & schedd );
    bool disconnect( bool commit_transaction, CondorError & errorStack );

    Qmgr_connection * q = NULL;
};


bool
QueueConnection::connect( const char * addr ) {
    DCSchedd schedd(addr);
    return schedd_connect(schedd);
}


bool
QueueConnection::schedd_connect( DCSchedd & schedd ) {
    q = ConnectQ(schedd);
    if( q == NULL ) {
        // This was HTCondorIOError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Failed to connect to schedd.");
        return false;
    }

    return true;
}


bool
QueueConnection::connect( DCSchedd & schedd ) {
    return schedd_connect(schedd);
}


bool
QueueConnection::disconnect( bool commit_transaction, CondorError & errorStack ) {
    bool committed = DisconnectQ(q, commit_transaction, &errorStack);
    if( commit_transaction && !committed ) {
        return false;
    }

    return true;
}


bool
QueueConnection::commit( std::string & message ) {
    CondorError errorStack;
    bool rv = disconnect(COMMIT_TRANSACTION, errorStack);
    if(! errorStack.empty()) {
        message = errorStack.message();
    }
    return rv;
}


bool
QueueConnection::abort() {
    CondorError errorStack;
    return disconnect(ABORT_TRANSACTION, errorStack);
}


QueueConnection::~QueueConnection() {
    // We're not just calling abort() because we may want to handle errors
    // differently here, in the implicit disconnect case.
    CondorError errorStack;
    (void)disconnect(ABORT_TRANSACTION, errorStack);
}
