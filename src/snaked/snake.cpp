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

int
Snake::HandleUnregisteredCommand(int command, Stream * sock) {
    dprintf( D_ALWAYS, "Forwarding command %d to Python.\n", command );


    // BEGIN GLORIOUS HACK
    std::string command_string;
    formatstr( command_string,
        "import snake\n"
        "g = snake.handleCommand(%d)\n",
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

    PyObject * eval = PyEval_EvalCode( blob, globals, locals );
    // END GLORIOUS HACK


    //
    // For the code fragment above, `eval` is consistently NULL, although
    // I would have expected it to be Py_None, instead.  Nonetheless,
    // we'll try to call the the generator `g` until something doesn't work.
    //


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

        eval = PyEval_EvalCode( invoke_generator_blob, globals, locals );
        if( eval == NULL ) { break; }


        PyObject * type = PyObject_Type(eval);
        PyObject * name = PyObject_GetAttrString(type, "__name__");
        // sigh... see py_str_to_std_string()
        PyObject * py_bytes = PyUnicode_AsUTF8String(name);
        char * buffer = NULL;
        Py_ssize_t size = -1;
        PyBytes_AsStringAndSize(py_bytes, &buffer, &size);
        dprintf( D_ALWAYS, "type(eval) = %s\n", buffer );


        PyObject * py_v_str = PyUnicode_FromString("v");
        PyObject * v = PyDict_GetItemWithError(locals, py_v_str);
        if( v == NULL ) {
            // We can fish out the exception later.
            dprintf( D_ALWAYS, "????\n" );
            break;
        }

        if( PyLong_Check(v) ) {
            dprintf( D_ALWAYS, "Python handler said %ld\n", PyLong_AsLong(v) );
        } else {
            dprintf( D_ALWAYS, "Unknown return from generator.\n" );
        }

    }


    return CLOSE_STREAM;
}
