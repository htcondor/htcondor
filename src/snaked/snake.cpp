#include "../python-bindings/condor_python.h"

#include "condor_common.h"
#include "condor_daemon_core.h"

#include <stdlib.h>
#include "snake.h"


bool
Snake::init() {
    // This is stupid, but the Python manual contradicts itself
    // as to how to safely use Py_SetPath() (which requires calling
    // Py_DecodeLocale(), which says it should never be called
    // directly), and I'm not presently interested in spending a lot
    // of time figuring out how to use Py_InitializeFromConfig().
    int OVERWRITE = 1;
    setenv( "PYTHONPATH", this->path.c_str(), OVERWRITE );

    Py_Initialize();
    return true;
}


Snake::~Snake() {
    Py_Finalize();
}


// This is a pretty dreadful hack just to get py_string_to_std_string().
// (Now also using py_new_classad2_classad(), but the point stands; functions
// used by both snaked and the version 2 bindings shouldn't be quite so
// clumsy to include.)
#include "../python-bindings/common2/py_handle.cpp"
#include "../python-bindings/common2/py_util.cpp"


PyObject * /* input */
read_py_tuple_from_sock( PyObject * pattern, ReliSock * sock ) {
}


int
write_py_tuple_to_sock( PyObject * output, ReliSock * sock ) {
    if(! PyTuple_Check(output)) {
        return -1;
    }

    Py_ssize_t size = PyTuple_Size(output);
    for( Py_ssize_t i = 0; i < size; ++i ) {
        PyObject * item = PyTuple_GetItem(output, i);

        if( item == Py_None ) {
            ;
        } else if(PyLong_Check(item)) {
            sock->encode();
            sock->put(PyLong_AsLong(item));
        } else if(PyFloat_Check(item)) {
            sock->encode();
            sock->put(PyFloat_AsDouble(item));
        } else if(PyUnicode_Check(item)) {
            std::string buffer;
            py_str_to_std_string(item, buffer);

            sock->encode();
            sock->put(buffer);
        } else if(PyBytes_Check(item)) {
            char * buffer = NULL;
            Py_ssize_t length = 0;
            PyBytes_AsStringAndSize(item, &buffer, &length);

            sock->encode();
            sock->put(buffer, length);
        } else if(py_is_classad2_classad(item)) {
            PyObject_Handle * handle = get_handle_from(item);
            ClassAd * classAd = (ClassAd *)handle->t;

            sock->encode();
            putClassAd(sock, * classAd);
        } else {
            dprintf( D_ALWAYS, "write_py_tuple_to_sock(): unsupported type for item %ld in tuple\n", i );
            return -2;
        }
    }

    return 0;
}


// This one should probably live in a common library, too.
int
py_object_to_repr_string( PyObject * o, std::string & r ) {
    if( o == NULL ) {
        return -1;
    }

    PyObject * py_repr = PyObject_Repr(o);
    if( py_repr != NULL ) {
        return py_str_to_std_string(py_repr, r);
    }

    return -1;
}


void
logPythonException() {
    PyObject * type = NULL;
    PyObject * value = NULL;
    PyObject * traceback = NULL;
    PyErr_Fetch( & type, & value, & traceback );

    // There's no C API for properly formatting an exception.
    PyObject * py_traceback_module = PyImport_ImportModule("traceback");
    ASSERT(py_traceback_module != NULL);

    PyObject * py_format_exception_f = PyObject_GetAttrString(
        py_traceback_module, "format_exception"
    );
    ASSERT(py_format_exception_f != NULL);
    ASSERT(PyCallable_Check(py_format_exception_f));

    PyObject * py_lines_list = PyObject_CallFunctionObjArgs(
        py_format_exception_f,
        type != NULL ? type : Py_None,
        value != NULL ? value : Py_None,
        traceback != NULL ? traceback : Py_None,
        NULL
    );
    ASSERT(py_lines_list != NULL);

    std::string log_entry = "Exception!\n\n";
    ASSERT(PyList_Check(py_lines_list));
    Py_ssize_t lineCount = PyList_Size(py_lines_list);
    for( Py_ssize_t i = 0; i < lineCount; ++i ) {
        // This is a borrowed reference.
        PyObject * py_line = PyList_GetItem(py_lines_list, i);
        ASSERT(PyUnicode_Check(py_line));

        std::string line;
        int rv = py_str_to_std_string( py_line, line );
        ASSERT(rv == 0);

        log_entry += line;
    }
    // The extra newlines are on purpose, because Python's "line"-internal
    // formatting makes assumptions about indentation.
    dprintf( D_ALWAYS, "%s\n", log_entry.c_str() );

    Py_DecRef(py_lines_list);
    Py_DecRef(py_format_exception_f);
    Py_DecRef(py_traceback_module);

    PyErr_Restore( type, value, traceback );

    Py_DecRef(traceback);
    Py_DecRef(value);
    Py_DecRef(type);
}


#include "dc_coroutines.h"

condor::dc::void_coroutine
callHandler( Snake * snake, int command, ReliSock * sock );

// Now misnamed, but there's enough uncertainty about how it will actually
// end up that there's no need to rename it quite yet.
int
Snake::HandleUnregisteredCommand( int command, Stream * sock ) {
    ReliSock * r = dynamic_cast<ReliSock *>(sock);
    if( r == NULL ) {
        dprintf( D_ALWAYS, "The snaked doesn't support UDP.\n" );
        return CLOSE_STREAM;
    }

    // When this coroutine calls co_await(), it will return through here,
    // which then returns back into the event loop.  We could add a
    // coroutine-type that returns an int, but I think it's actually easier
    // just to have the coroutine manage the socket's lifetime itself.
    callHandler(this, command, r);
    return KEEP_STREAM;
}


condor::dc::void_coroutine
callHandler( Snake * snake, int command, ReliSock * r ) {
    // Read the payload off the wire.  In general, this should actually
    // be done after obtaining the generator, so that we check for a
    // payload every time through the generator-invocation loop (if a
    // reply is expected, as indicated by the previous invocation's

    // return value).
    r->decode();
    ClassAd payload;
    if(! getClassAd(r, payload)) {
        dprintf( D_ALWAYS, "Failed to read ClassAd payload.\n" );
        delete r;
        co_return;
    }
    if(! r->end_of_message()) {
        dprintf( D_ALWAYS, "Failed to read end-of-message.\n" );
        delete r;
        co_return;
    }

    dprintf( D_ALWAYS, "Calling snake(%p).handleCommand(%d)...\n", snake, command );

    //
    // What we really want is for Python-side handleCommand() function to
    // be a coroutine; what we have are generators.  We therefore cheat a
    // a lot. ;)
    //
    // 1.  Execute some Python to import the modules and create the
    //     generator we want to use.  We phrase this as assigning the
    //     generator to a variable with a known (constant) name, because
    //     we can look locals up after the fact, but the result of
    //     PyEval_EvalCode() is NULL.  (It seems like it should be Py_None,
    //     but that might be a Python-language thing.  It is certainly not,
    //     as one might expect from bash or the Python interactive console,
    //     the result of the last line of the code.
    // 2.  The generator is passed the (invariant) command int and the
    //     `payload` variable.  The reference is copied (as it always is),
    //     and then kept alive by the generator object.  However, since
    //     we created the payload variable on this side, it's easy for
    //     us to update the referenced object -- for now a dict, but it
    //     should be a tuple -- every time before we call the generator.
    //     We could have made this a global, but that would have prevented
    //     (well, made more difficult) interlacing different generators.
    // 3.  Invoke the generator.  For the reasons(s) above, assign the
    //     result into a known (constant) name.
    // 4.  Check for a StopIteration exception, indicating that the
    //     generator has completed; if it has, we're done.
    // 5.  Otherwise -- if there wasn't some other exception -- extract
    //     the return value.  Right now, the return tuple is fixed, but
    //     we could generalize it so that we just send the tuple's
    //     components appropriately.
    // 6.  Read the reply.  For full generality, the return should be
    //     (reply-tuple, timeout, response-tuple), where we go back to
    //     the event loop for the timeout after sending the reply-tuple
    //     and try to read the response-tuple when the socket goes hot.
    //     We can probably implement this with a simple variant of the
    //     AwaitableDeadlineReaper.
    //


    // Theoretically, we could store this command string in the config
    // system, but that seems like a terrible idea.  We could also --
    // at considerable cost in time, effort, and clarity -- rewrite
    // this as a series of C API calls.
    std::string command_string;
    formatstr( command_string,
        "import classad2\n"
        "import snake\n"
        "g = snake.handleCommand(%d, payload)\n",
        command
    );
    // This is a new reference.
    PyObject * blob = Py_CompileString(
        command_string.c_str(),
        "snaked", /* the filename to use in error messages */
        Py_file_input
    );
    if( blob == NULL ) {
        logPythonException();
        PyErr_Clear();

        delete r;
        co_return;
    }

    // This a borrowed reference, but presumably the __main__ module
    // won't be destroyed until the interpreter is.
    PyObject * main_module = PyImport_AddModule("__main__");
    // This is allowed to fail, but there's nothing sane we can do if it does.
    ASSERT(main_module != NULL);
    // Thi is also a borrowed reference, with the same lifetime.
    PyObject * globals = PyModule_GetDict(main_module);
    // This is allowed to fail, but there's nothing sane we can do if it does.
    ASSERT(globals != NULL);

    // This is a new reference.
    PyObject * locals = PyDict_New();
    // This is allowed to fail, but there's nothing sane we can do if it does.
    ASSERT(locals != NULL);

    PyObject * py_payload = PyDict_New();
    // This is allowed to fail, but there's nothing sane we can do if it does.
    ASSERT(py_payload != NULL);

    PyObject * py_classad = py_new_classad2_classad(& payload);
    // This is allowed to fail, but there's nothing sane we can do if it does.
    ASSERT(py_classad != NULL);

    // This function does _not_ steal a reference to py_payload.
    int rv = PyDict_SetItemString(locals, "payload", py_payload);
    ASSERT(rv == 0);

    // Returns a new reference if it returns anything at all.
    PyObject * eval = PyEval_EvalCode( blob, globals, locals );
    if( eval != NULL ) {
        if( eval != Py_None ) {
            std::string repr = "<null>";
            py_object_to_repr_string(eval, repr);
            dprintf( D_ALWAYS, "PyEval_EvalCode() unexpectedly returned something (%s); decrementing its refcount.\n", repr.c_str() );
        }

        Py_DecRef(eval);
        eval = NULL;
    }

    // This isn't a loop variable because if it were, we'd have to decrement
    // its refcount before we exited the loop, and we don't want to do that,
    // because we need it to keep the exception-handling reference(s) alive.
    PyObject * invoke_generator_blob = NULL;
    while( PyErr_Occurred() == NULL ) {
        //
        // We can't do `next(g)` here because that consisently returns
        // Py_None, which seems wrong, because it's different from what
        // the interactive Python console does.
        //
        // Note that we can't `return next(g)` here because there's no
        // previous frame; the interpreter will segfault.
        //
        // Assigning to a local is clumsy, but it does actually work.
        //
        std::string invoke_generator_string = "v = next(g)";
        invoke_generator_blob = Py_CompileString(
            invoke_generator_string.c_str(),
            "snaked (generator)",
            Py_file_input
        );

        // At some point, we'll call getClassAd() before each invocation
        // of the generator.

        // This function does _not_ steal a reference to py_payload.
        rv = PyDict_SetItemString(py_payload, "classad", py_classad);
        ASSERT(rv == 0);

        eval = PyEval_EvalCode( invoke_generator_blob, globals, locals );
        if( eval == NULL ) {
            // Then an exception occurred and we handle it below.  We
            // don't Py_DecRef(invoke_generator_blob) here because that
            // almost certainly removes the last reference to the
            // exception object(s) we'll need below.
            dprintf( D_ALWAYS, "The generator threw an exception.\n" );
            break;
        }

        PyObject * py_v_str = PyUnicode_FromString("v");
        // Returns a borrowed reference.
        PyObject * v = PyDict_GetItemWithError(locals, py_v_str);
        Py_DecRef(py_v_str);
        py_v_str = NULL;
        if( v == NULL ) {
            logPythonException();

            Py_DecRef(invoke_generator_blob);
            Py_DecRef(blob);

            delete r;
            co_return;
        }

        // We can now handle many tuples.  Our fully-generic goal assumes
        // the generator will return a tuple of (reply-tuple, timeout,
        // payload-template), but that's for a bit later.
        if( PyTuple_Check(v) ) {
            int rv = write_py_tuple_to_sock(v, r);
            if(rv != 0) {
                dprintf( D_ALWAYS, "failed to write Python tuple to socket\n" );

                Py_DecRef(invoke_generator_blob);
                Py_DecRef(blob);

                delete r;
                co_return;
            }
        } else {
            dprintf( D_ALWAYS, "generator must yield tuples\n" );

            Py_DecRef(invoke_generator_blob);
            Py_DecRef(blob);

            delete r;
            co_return;
        }

        Py_DecRef(invoke_generator_blob);
        invoke_generator_blob = NULL;

        // We read the first payload "by hand" already; see README.md for
        // what we could do more generally, which would move this.
        condor::dc::AwaitableDeadlineSocket hotOrNot;
        hotOrNot.deadline(r, 2);
        auto [socket, timed_out] = co_await(hotOrNot);
        ASSERT(socket == r);
        if( timed_out ) {
            dprintf( D_ALWAYS, "timed out waiting for reply, but that's OK for this test.\n" );
        }
    }

    if( PyErr_Occurred() != NULL ) {
            if( PyErr_ExceptionMatches( PyExc_StopIteration ) ) {
                dprintf( D_ALWAYS, "The exception was a StopIteration, as expected.\n" );

                // If we don't clear the StopIterator, nobody will.
                PyErr_Clear();

                //
                // This is actually the expected exit from this function,
                // which is annoying.
                //
                r->end_of_message();

                Py_DecRef(invoke_generator_blob);
                // The other Python object we created -- the locals, the
                // payload variable, and its single entry -- all end up
                // owned the blob, so we only decrement the refcount on
                // it, because Python has no way to know when we're done
                // with it.
                Py_DecRef(blob);

                delete r;
                co_return;
            }

            logPythonException();
            PyErr_Clear();

            Py_DecRef(invoke_generator_blob);
            Py_DecRef(blob);

            delete r;
            co_return;
    } else {
        dprintf( D_ALWAYS, "How did we get here?\n" );

        Py_DecRef(invoke_generator_blob);
        Py_DecRef(blob);

        delete r;
        co_return;
    }
}
