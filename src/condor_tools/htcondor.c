/***************************************************************
 *
 * Copyright (C) 2024, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#if defined(_MSC_VER)
 #pragma push_macro("PLATFORM")
 #undef PLATFORM

 #pragma push_macro("_DEBUG")
 #undef _DEBUG
#endif

 // if you enable limited api, then you have to also change the cmake to link with python3.lib
 // #define Py_LIMITED_API 0x03060000
#include <Python.h>

#if defined(_MSC_VER)
  #pragma pop_macro("_DEBUG")
  #pragma pop_macro("PLATFORM")
#endif

/*
const char * program = "import sys\n"
		"from htcondor_cli.cli import main\n"
;
*/

int main(const int argc, char * argv[]) {
	int ret = 0;

	PyConfig config;
	PyConfig_InitPythonConfig(&config);

	// since the arg parser expects python.exe to be argv[0], and htcondor to be argv[1]
	// we have to insert a dummy "htcondor" into the args between argv[0] and argv[1]
	int mangled_argc = argc + 1;
	char ** mangled_argv = malloc(sizeof(char*) * argc+2);
	for (int jj=0, ii=0; ii < argc; ++ii) {
		mangled_argv[jj++] = argv[ii];
		if (ii == 0) { mangled_argv[jj++] = "htcondor"; }
	}

	// set our custom argc/argv and then initialize the interpreter
	PyStatus status = PyConfig_SetBytesArgv(&config, mangled_argc, mangled_argv);
	if ( ! PyStatus_Exception(status)) {
		status = Py_InitializeFromConfig(&config);
	}
	if (PyStatus_Exception(status)) {
		PyConfig_Clear(&config);
		if (PyStatus_IsExit(status)) {
			return status.exitcode;
		} else {
			Py_ExitStatusException(status);
			return status.exitcode;
		}
	}

	// "import sys"
	// we use AddModule rather than ImportModule here because sys is always loaded.
	//PyObject * sys = PyImport_AddModule("sys");

	// "from htcondor_cli.cli import main"
	PyObject * cli_import_list = PyList_New(1);
	PyList_Append(cli_import_list, PyUnicode_FromString("main"));
	PyObject * cli = PyImport_ImportModuleLevel("htcondor_cli.cli", NULL, NULL, cli_import_list, 0);
	Py_DECREF(cli_import_list); cli_import_list = NULL;

	// "result = main()"
	PyObject * fnmain = PyObject_GetAttrString(cli, "main");
	if ( ! fnmain || ! PyCallable_Check(fnmain)) {
		fprintf(stderr, "htcondor_cli.cli does not have a valid main method\n");
		ret = -1;
	} else {
		PyObject * result = PyObject_CallObject(fnmain, NULL);
		// unpack the main return value
		ret = _PyLong_AsInt(result);
	}

	// free stuff
	Py_DECREF(fnmain);
	Py_DECREF(cli);
	//Py_DECREF(sys);
	free(mangled_argv);

	// Finalize the Python Interpreter
	Py_Finalize();

	return ret;
}

