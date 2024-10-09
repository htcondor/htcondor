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
read_py_tuple_from_sock( PyObject * pattern, ReliSock * sock, PyObject * py_end_of_message ) {
    if(! PyTuple_Check(pattern)) {
        return NULL;
    }

    Py_ssize_t size = PyTuple_Size(pattern);
    PyObject * input = PyTuple_New(size);
    ASSERT(input != NULL);

    for( Py_ssize_t i = 0; i < size; ++i ) {
        PyObject * item = PyTuple_GetItem(pattern, i);

        if( item == Py_None ) {
            ;
        } else if( item == py_end_of_message ) {
            sock->decode();
            sock->end_of_message();
        } else if(PyLong_Check(item)) {
            sock->decode();
            long item;
            sock->get(item);

            PyTuple_SetItem(input, i, PyLong_FromLong(item));
        } else if(PyFloat_Check(item)) {
            sock->decode();
            double item;
            sock->get(item);

            PyTuple_SetItem(input, i, PyFloat_FromDouble(item));
        } else if(PyUnicode_Check(item)) {
            sock->decode();
            std::string item;
            sock->get(item);

            PyTuple_SetItem(input, i, PyUnicode_FromString(item.c_str()));
        } else if(PyBytes_Check(item)) {
            Py_ssize_t length = PyBytes_Size(item);

            sock->decode();
            char * buffer = NULL;
            sock->get(buffer, length);
            PyObject * item = PyBytes_FromStringAndSize(buffer, length);

            PyTuple_SetItem(input, i, item);
        } else if(py_is_classad2_classad(item)) {
            sock->decode();
            ClassAd * classAd = new ClassAd();
            getClassAd(sock, * classAd);
            PyObject * item = py_new_classad2_classad(classAd);

            PyTuple_SetItem(input, i, item);
        } else {
            dprintf( D_ALWAYS, "write_py_tuple_to_sock(): unsupported type for item %ld in tuple\n", i );
            return NULL;
        }
    }

    return input;
}


int
write_py_tuple_to_sock( PyObject * output, ReliSock * sock, PyObject * py_end_of_message ) {
    if(! PyTuple_Check(output)) {
        return -1;
    }

    Py_ssize_t size = PyTuple_Size(output);
    for( Py_ssize_t i = 0; i < size; ++i ) {
        PyObject * item = PyTuple_GetItem(output, i);

        if( item == Py_None ) {
            ;
        } else if( item == py_end_of_message ) {
            sock->encode();
            sock->end_of_message();
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

    std::string log_entry = "Exception!\n\n";
    if( py_lines_list != NULL ) {
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
    } else {
        std::string type_repr = "<null>";
        py_object_to_repr_string(type, type_repr);
        std::string value_repr = "<null>";
        py_object_to_repr_string(value, value_repr);
        log_entry +=  type_repr + "\n" + value_repr + "\n";
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
call_command_handler( Snake * snake, const char * which_python_function,
             int command, ReliSock * sock );


int
Snake::CallCommandHandler( const char * which_python_function,
    int command, Stream * sock
) {
    ReliSock * r = dynamic_cast<ReliSock *>(sock);
    if( r == NULL ) {
        dprintf( D_ALWAYS, "The snaked doesn't support UDP.\n" );
        return CLOSE_STREAM;
    }

    // When this coroutine calls co_await(), it will return through here,
    // which then returns back into the event loop.  We could add a
    // coroutine-type that returns an int, but I think it's actually easier
    // just to have the coroutine manage the socket's lifetime itself.
    call_command_handler(this, which_python_function, command, r);
    return KEEP_STREAM;
}


//
// Evaluates the given string of Python in a brand-new context
// and returns the resulting blob, main module, globals, and
// locals.  The "command string" must _not_ have a result.
//
// The blob will be a new reference and must be Py_DecRef()d
// appropriately.  The main module and globals are owned by
// the blob and must NOT be Py_DecRef()d.  The locals will be
// a new reference and must be Py_DecRef()d appropriately.
//
std::tuple<PyObject *, PyObject *, PyObject *, PyObject *>
invoke_new_command_string( const std::string & command_string );

condor::dc::void_coroutine
call_command_handler(
  Snake * snake, const char * which_python_function,
  int command, ReliSock * r
) {
    dprintf( D_ALWAYS, "Calling snake(%p).%s(%d)...\n", snake, which_python_function, command );

    std::string command_string;
    formatstr( command_string,
        "import classad2\n"
        "import snake\n"
        // Make the end-of-message flag available to handleCommand().
        "snake.end_of_message = object()\n"
        // Make the end-of-message flag easier for the C code to find.
        "end_of_message = snake.end_of_message\n"
        "g = snake.%s(%d)\n",
        which_python_function,
        command
    );

    auto [blob, main_module, globals, locals] =
        invoke_new_command_string( command_string );

    if( blob == NULL ) {
        dprintf( D_ALWAYS, "Initial blob generation raised an exception:\n" );
        logPythonException();
        PyErr_Clear();

        delete r;
        co_return;
    }


    PyObject * py_end_of_message = PyDict_GetItemString(locals, "end_of_message");
    ASSERT(py_end_of_message != NULL);

    // This isn't a loop variable because if it were, we'd have to decrement
    // its refcount before we exited the loop, and we don't want to do that,
    // because we need it to keep the exception-handling reference(s) alive.
    PyObject * invoke_generator_blob = NULL;
    // We can't send() anything on the first invocation, because there's
    // no yield() to return it from (yet).
    std::string invoke_generator_string = "v = next(g)";
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
        invoke_generator_blob = Py_CompileString(
            invoke_generator_string.c_str(),
            "snaked (generator)",
            Py_file_input
        );

        PyObject * eval = PyEval_EvalCode( invoke_generator_blob, globals, locals );
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
            dprintf( D_ALWAYS, "PyDict_GetItemWithError() failed:\n" );
            logPythonException();
            PyErr_Clear();

            Py_DecRef(invoke_generator_blob);
            Py_DecRef(blob);
            Py_DecRef(locals);

            delete r;
            co_return;
        }

        // The generator is expected to return tye tuple
        //     (reply-format, time-out, input-format)
        // where the format variables are tuples consisting only of
        // ints, floats, strings, bytes, and classad2.ClassAds.
        if( PyTuple_Check(v) && PyTuple_Size(v) == 3 ) {
            PyObject * output = PyTuple_GetItem(v, 0);
            PyObject * py_time_out = PyTuple_GetItem(v, 1);
            PyObject * input_format = PyTuple_GetItem(v, 2);

            if( output == Py_None ) {
                // Don't send an end-of-message.
            } else if( PyTuple_Check(output) ) {
/*
                std::string repr = "<null>";
                py_object_to_repr_string(output, repr);
                dprintf( D_ALWAYS, "sending output: %s\n", repr.c_str() );
*/

                int rv = write_py_tuple_to_sock(output, r, py_end_of_message);
                if(rv != 0) {
                    dprintf( D_ALWAYS, "failed to write Python tuple to socket\n" );

                    Py_DecRef(invoke_generator_blob);
                    Py_DecRef(blob);
                    Py_DecRef(locals);

                    delete r;
                    co_return;
                }
            } else {
                dprintf( D_ALWAYS, "generator yielded an invalid output specifier\n" );

                Py_DecRef(invoke_generator_blob);
                Py_DecRef(blob);
                Py_DecRef(locals);

                delete r;
                co_return;
            }

            if( py_time_out == Py_None ) {
                // Don't go through the event loop before attempting this read.
            } else if( PyLong_Check(py_time_out) ) {
                long time_out = PyLong_AsLong(py_time_out);
                condor::dc::AwaitableDeadlineSocket hotOrNot;
                hotOrNot.deadline(r, time_out);
                auto [socket, timed_out] = co_await(hotOrNot);
                ASSERT(socket == r);

                if( timed_out ) {
                    dprintf( D_ALWAYS, "Timed out, skipping payload read.\n" );
                    // If the command handler wanted to block reading the
                    // next payload, it would have said so.  Instead, we
                    // resume, the next time around the loop, with None as
                    // the payload.  (We could also signal the time-out
                    // explicitly, but
                    input_format = Py_None;
                }
            } else {
                dprintf( D_ALWAYS, "generator yielded an invalid time-out specifier\n" );

                Py_DecRef(invoke_generator_blob);
                Py_DecRef(blob);
                Py_DecRef(locals);

                delete r;
                co_return;
            }

            if( input_format == Py_None ) {
                PyDict_SetItemString(locals, "payload", Py_None);
            } else if( PyTuple_Check(input_format) ) {
                PyObject * input = read_py_tuple_from_sock(input_format, r, py_end_of_message);

                if( input == NULL ) {
                    dprintf( D_ALWAYS, "failed to read Python tuple from socket\n" );

                    Py_DecRef(invoke_generator_blob);
                    Py_DecRef(blob);
                    Py_DecRef(locals);

                    delete r;
                    co_return;
                }

/*
                std::string repr = "<null>";
                py_object_to_repr_string(input, repr);
                dprintf( D_ALWAYS, "read input: %s\n", repr.c_str() );
*/

                PyDict_SetItemString(locals, "payload", input);
            } else {
                dprintf( D_ALWAYS, "generator yielded an invalid input specifier\n" );

                Py_DecRef(invoke_generator_blob);
                Py_DecRef(blob);
                Py_DecRef(locals);

                delete r;
                co_return;
            }
        } else {
            dprintf( D_ALWAYS, "generator must yield ((output), timeout, (input-format))\n" );

            Py_DecRef(invoke_generator_blob);
            Py_DecRef(blob);
            Py_DecRef(locals);

            delete r;
            co_return;
        }

        Py_DecRef(invoke_generator_blob);
        invoke_generator_blob = NULL;

        invoke_generator_string = "v = g.send(payload)";
    }

    if( PyErr_Occurred() != NULL ) {
            if( PyErr_ExceptionMatches( PyExc_StopIteration ) ) {
                dprintf( D_ALWAYS, "The exception was a StopIteration, as expected.\n" );

                // If we don't clear the StopIterator, nobody will.
                PyErr_Clear();

                Py_DecRef(invoke_generator_blob);
                // The other Python object we created -- the locals, the
                // payload variable, and its single entry -- all end up
                // owned the blob, so we only decrement the refcount on
                // it, because Python has no way to know when we're done
                // with it.
                Py_DecRef(blob);
                Py_DecRef(locals);

                delete r;
                co_return;
            }

            logPythonException();
            PyErr_Clear();

            Py_DecRef(invoke_generator_blob);
            Py_DecRef(blob);
            Py_DecRef(locals);

            delete r;
            co_return;
    } else {
        dprintf( D_ALWAYS, "How did we get here?\n" );

        Py_DecRef(invoke_generator_blob);
        Py_DecRef(blob);
        Py_DecRef(locals);

        delete r;
        co_return;
    }
}


int
Snake::CallUpdateDaemonAd( const std::string & genus ) {
    const char * which_python_function = "updateDaemonAd";

    dprintf( D_ALWAYS, "[timer] Calling snake.%s()...\n", which_python_function );

    PyObject * py_snake_module = PyImport_ImportModule( "snake" );
    if( py_snake_module == NULL ) {
        dprintf( D_ALWAYS, "[timer]  Failed to import classad module, aborting.\n" );

        logPythonException();
        PyErr_Clear();

        return -1;
    }

    PyObject * py_function = PyObject_GetAttrString(
        py_snake_module, which_python_function
    );
    if( py_function == NULL ) {
        dprintf( D_ALWAYS, "[timer] Did not find snake.%s(), aborting.\n", which_python_function );

        logPythonException();
        PyErr_Clear();

        Py_DecRef(py_snake_module);
        return -1;
    }


    ClassAd daemonAd;
    daemonCore->publish(& daemonAd);
    daemonAd.InsertAttr( "MyType", genus.c_str() );

    ClassAd * pythonAd = new ClassAd(daemonAd);
    PyObject * py_ad = py_new_classad2_classad(pythonAd);
    if( py_ad == NULL ) {
        dprintf( D_ALWAYS, "[timer] Failed to construct ClassAd argument, aborting.\n" );

        logPythonException();
        PyErr_Clear();

        Py_DecRef(py_function);
        Py_DecRef(py_snake_module);
        return -1;
    }


    PyObject * py_result = PyObject_CallFunctionObjArgs(
        py_function, py_ad, NULL
    );
    if( py_result == NULL ) {
        dprintf( D_ALWAYS, "[timer] Call to snake.%s() failed, aborting.\n", which_python_function );

        logPythonException();
        PyErr_Clear();

        Py_DecRef(py_ad);
        Py_DecRef(py_function);
        Py_DecRef(py_snake_module);
        return -1;
    }


    if(! py_is_classad2_classad(py_result)) {
        dprintf( D_ALWAYS, "[timer]  Python function did not return a classad2.ClassAd.\n" );

        Py_DecRef(py_result);
        Py_DecRef(py_ad);
        Py_DecRef(py_function);
        Py_DecRef(py_snake_module);

        return 1;
    }

    PyObject_Handle * handle = get_handle_from(py_result);
    ClassAd * classAd = (ClassAd *)handle->t;

    classAd->Update(daemonAd);

    bool should_block = false;
    int rv = daemonCore->sendUpdates( UPDATE_AD_GENERIC,
        classAd, NULL, should_block
    );


    Py_DecRef(py_result);
    Py_DecRef(py_ad);
    Py_DecRef(py_function);
    Py_DecRef(py_snake_module);

    return rv;
}


condor::dc::void_coroutine
call_polling_handler( const char * which_python_function, int which_signal );


void
Snake::CallPollingHandler( const char * which_python_function, int which_signal ) {
    call_polling_handler( which_python_function, which_signal );
}


condor::dc::void_coroutine
call_polling_handler( const char * which_python_function, int which_signal ) {
    ASSERT(which_python_function != NULL );
    ASSERT(which_signal != -1);

    std::string command_string;
    formatstr( command_string,
        "import snake\n"
        "g = snake.%s()\n",
        which_python_function
    );
    auto [make_generator_blob, main_module, globals, locals] =
        invoke_new_command_string( command_string );
    if( make_generator_blob == NULL ) {
        dprintf( D_ALWAYS, "Initial blob generation raised an exception:\n" );
        logPythonException();
        PyErr_Clear();

        co_return;
    }

    std::string invoke_generator_string = "v = next(g)";
    // invoke_next_command_string( invoke_generator_string, globals, locals );

/*
        // If the generator is done, exit.

        // Otherwise, the generator must have returned the dealy it
        // wants before being called again.  Construct an
        // AwaitableDeadlineSignal and block on it.  (Yes, we want
        // a new one every time through the loop.)
        AwaitableDeadlineSignal blocker;
        blocker.deadline( which_signal, delay );
        auto [the_signal, did_time_out] = co_await(blocker);
        calling_after_time_out = did_time_out;
*/

}


std::tuple<PyObject *, PyObject *, PyObject *, PyObject *>
invoke_new_command_string( const std::string & command_string ) {

    // This is a new reference.
    PyObject * blob = Py_CompileString(
        command_string.c_str(),
        "snaked", /* the filename to use in error messages */
        Py_file_input
    );

    if( blob == NULL ) {
        return {NULL, NULL, NULL, NULL};
    }


    // This is a borrowed reference.
    PyObject * main_module = PyImport_AddModule("__main__");
    ASSERT(main_module != NULL);

    // This is a borrowed reference.
    PyObject * globals = PyModule_GetDict(main_module);
    ASSERT(globals != NULL);

    // This is a new reference.
    PyObject * locals = PyDict_New();
    ASSERT(locals != NULL);

    // This is a new reference.
    PyObject * eval = PyEval_EvalCode( blob, globals, locals );
    if( eval == NULL ) {
        dprintf( D_ALWAYS, "PyEval_EvalCode() failed, aborting.\n" );
        logPythonException();
        PyErr_Clear();
    } else if( eval != Py_None ) {
        std::string repr = "<null>";
        py_object_to_repr_string(eval, repr);
        dprintf( D_ALWAYS, "PyEval_EvalCode() unexpectedly returned something (%s); aborting.\n", repr.c_str() );
    }
    ASSERT(eval == Py_None);


    Py_DecRef(blob);
    Py_DecRef(locals);

    return {blob, main_module, globals, locals};
}
