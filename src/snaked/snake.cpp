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
        "snake.handleCommand(%d)\n",
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

    return CLOSE_STREAM;
}
