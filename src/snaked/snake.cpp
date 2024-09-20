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

// This is a pretty dreadful hack just to py_string_to_std_string().
#include "../python-bindings/common2/py_handle.cpp"
#include "../python-bindings/common2/py_util.cpp"

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


int
Snake::HandleUnregisteredCommand(int command, Stream * /* sock */ ) {
    dprintf( D_ALWAYS, "Forwarding command %d to Python.\n", command );


    // BEGIN GLORIOUS HACK
    std::string command_string;
    formatstr( command_string,
        "import classad2\n"
        "import snake\n"
        "g = snake.handleCommand(%d, payload)\n",
        command
    );
    PyObject * blob = Py_CompileString(
        command_string.c_str(),
        "snaked", /* the filename to use in error messages */
        Py_file_input
    );

    PyObject * main_module = PyImport_AddModule("__main__");
    PyObject * globals = PyModule_GetDict(main_module);
    PyObject * locals = PyDict_New();

    PyObject * py_payload = PyDict_New();
    PyObject * py_classad = PyUnicode_FromString( "payload" );
    PyDict_SetItemString(py_payload, "classad", py_classad);
    PyDict_SetItemString(locals, "payload", py_payload);

    PyObject * eval = PyEval_EvalCode( blob, globals, locals );
    // END GLORIOUS HACK

    // Obviously, there's a lot to do in terms of cleaning up the
    // various PyObjects involved, but also try to figure out if
    // PyDict_SetItemString() requires any explicit refcounting.

    //
    // BEGIN ADDITIONAL HACKING
    //
    // For the code fragment above, `eval` is consistently NULL, although
    // I would have expected it to be Py_None, instead.  Nonetheless,
    // we'll try to call the the generator `g` until something doesn't work.
    //

    const char * payloads[] = {
        "alpha",
        "beta",
        "gamma"
    };
    for( int i = 0; PyErr_Occurred() == NULL; ++i ) {
        dprintf( D_ALWAYS, "Invoking generator...\n" );
        if( i >= 3 ) { i = 0; }

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

        PyObject * py_classad = PyUnicode_FromString( payloads[i] );
        PyDict_SetItemString(py_payload, "classad", py_classad);

        eval = PyEval_EvalCode( invoke_generator_blob, globals, locals );
        if( eval == NULL ) { break; }

        PyObject * py_v_str = PyUnicode_FromString("v");
        PyObject * v = PyDict_GetItemWithError(locals, py_v_str);
        if( v == NULL ) {
            PyObject * type = NULL;
            PyObject * value = NULL;
            PyObject * traceback = NULL;
            PyErr_Fetch( & type, & value, & traceback );

            // If this ever actually happens in practice, we can figure
            // out how to actually print something useful then.
            dprintf( D_ALWAYS, "Failed to obtain callback's return value, aborting handler.\n" );

            PyErr_Restore( type, value, traceback );
            return CLOSE_STREAM;
        }

        if( PyLong_Check(v) ) {
            dprintf( D_ALWAYS, "Python handler said %ld\n", PyLong_AsLong(v) );
        } else {
            dprintf( D_ALWAYS, "Unknown return from generator.\n" );
        }
    }

    if( PyErr_Occurred() != NULL ) {
            PyObject * type = NULL;
            PyObject * value = NULL;
            PyObject * traceback = NULL;
            PyErr_Fetch( & type, & value, & traceback );

            dprintf( D_ALWAYS, "Exception!\n" );

            std::string type_r = "<null>";
            py_object_to_repr_string(type, type_r);
            dprintf( D_ALWAYS, "\ttype = %s\n", type_r.c_str() );

            std::string value_r = "<null>";
            py_object_to_repr_string(value, value_r);
            dprintf( D_ALWAYS, "\tvalue = %s\n", value_r.c_str() );

            std::string traceback_r = "<null>";
            py_object_to_repr_string(traceback, traceback_r);
            dprintf( D_ALWAYS, "\ttraceback = %s\n", traceback_r.c_str() );

            PyErr_Restore( type, value, traceback );
            return CLOSE_STREAM;
    }

    //
    // END ADDITIONAL HACKING
    //


    return CLOSE_STREAM;
}
