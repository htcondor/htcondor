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


// This one should probably live a common library, too.
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

/*
    // This is _almost_ exactly what we want.
    PyObject * py_log_text = PyObject_Str(py_lines_list);
    ASSERT(py_log_text != NULL);

    std::string log_text;
    int rv = py_str_to_std_string( py_log_text, log_text );
    ASSERT(rv == 0);
    dprintf( D_ALWAYS, "%s\n", log_text.c_str() );

    Py_DecRef(py_log_text);
*/

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


// Now misnamed, but there's enough uncertainty about how it will actually
// end up that there's no need to rename it quite yet.
int
Snake::HandleUnregisteredCommand( int command, Stream * sock ) {
    ReliSock * r = dynamic_cast<ReliSock *>(sock);
    if( r == NULL ) {
        dprintf( D_ALWAYS, "The snaked doesn't support UDP.\n" );
        return CLOSE_STREAM;
    }

    // Read the payload off the wire.  In general, this should actually
    // be done after obtaining the generator, so that we check for a
    // payload every time through the generator-invocation loop (if a
    // reply is expected, as indicated by the previous invocation's

    // return value).
    r->decode();
    ClassAd payload;
    if(! getClassAd(r, payload)) {
        dprintf( D_ALWAYS, "Failed to read ClassAd payload.\n" );
        return CLOSE_STREAM;
    }
    if(! r->end_of_message()) {
        dprintf( D_ALWAYS, "Failed to read end-of-message.\n" );
        return CLOSE_STREAM;
    }

    dprintf( D_ALWAYS, "Calling snake.handleCommand(%d)...\n", command );

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
        return CLOSE_STREAM;
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
        PyObject * invoke_generator_blob = Py_CompileString(
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
            break;
        }

        PyObject * py_v_str = PyUnicode_FromString("v");
        // Returns a borrowed reference.
        PyObject * v = PyDict_GetItemWithError(locals, py_v_str);
        if( v == NULL ) {
            logPythonException();

            Py_DecRef(invoke_generator_blob);
            Py_DecRef(blob);

            return CLOSE_STREAM;
        }

        // We expect v to be a (int, classad2.ClassAd) tuple.
        if( PyTuple_Check(v) ) {
            // Returns a borrowed reference.
            PyObject * py_reply_i = PyTuple_GetItem(v, 0);
            // Returns a borrowed reference.
            PyObject * py_reply_c = PyTuple_GetItem(v, 1);

            if(! PyLong_Check(py_reply_i)) {
                dprintf( D_ALWAYS, "first in tuple not an int\n" );
                Py_DecRef(invoke_generator_blob);
                invoke_generator_blob = NULL;
                continue;
            }
            long int replyInt = PyLong_AsLong(py_reply_i);

            r->encode();
            // FIXME: error-checking.
            r->code(replyInt);

            if( py_is_classad2_classad(py_reply_c) ) {
                PyObject_Handle * handle = get_handle_from(py_reply_c);
                ClassAd * replyAd = (ClassAd *)handle->t;

                // FIXME: error-checking.
                putClassAd(r, * replyAd);
            } else if ( py_reply_c == Py_None ) {
                ;
            } else {
                dprintf( D_ALWAYS, "second in tuple not classad2.ClassAd or None\n" );
                Py_DecRef(invoke_generator_blob);
                invoke_generator_blob = NULL;
                continue;
            }
        } else {
            dprintf( D_ALWAYS, "unrecognized return type from generator\n" );
            Py_DecRef(invoke_generator_blob);
            invoke_generator_blob = NULL;
            continue;
        }

        Py_DecRef(invoke_generator_blob);
        invoke_generator_blob = NULL;
    }

    if( PyErr_Occurred() != NULL ) {
            if( PyErr_ExceptionMatches( PyExc_StopIteration ) ) {
                // If we don't clear the StopIterator, nobody will.
                PyErr_Clear();

                //
                // This is actually the expected exit from this function,
                // which is annoying.
                //
                r->end_of_message();

                // The other Python object we created -- the locals, the
                // payload variable, and its single entry -- all end up
                // owned the blob, so we only decrement the refcount on
                // it, because Python has no way to know when we're done
                // with it.
                Py_DecRef(blob);

                return CLOSE_STREAM;
            }

            logPythonException();
            PyErr_Clear();

            Py_DecRef(blob);

            return CLOSE_STREAM;
    } else {
        dprintf( D_ALWAYS, "How did we get here?\n" );

        Py_DecRef(blob);

        return CLOSE_STREAM;
    }
}
