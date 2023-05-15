static PyObject_Handle *
get_handle_from(PyObject * pyClassAd) {
	return (PyObject_Handle *)PyObject_GetAttrString( pyClassAd, "_handle" );
}


static PyObject *
py_new_classad_classad(void * classAd) {
	static PyObject * py_htcondor_module = NULL;
	if( py_htcondor_module == NULL ) {
		 py_htcondor_module = PyImport_ImportModule( "htcondor" );
	}

	static PyObject * py_htcondor_classad_module = NULL;
	if( py_htcondor_classad_module == NULL ) {
		py_htcondor_classad_module = PyObject_GetAttrString( py_htcondor_module, "classad" );
	}

	static PyObject * py_ClassAd_class = NULL;
	if( py_ClassAd_class == NULL ) {
		py_ClassAd_class = PyObject_GetAttrString( py_htcondor_classad_module, "ClassAd" );
	}

	PyObject * pyClassAd = PyObject_CallObject(py_ClassAd_class, NULL);
	auto * handle = get_handle_from(pyClassAd);
	handle->t = (void *)classAd;

	return pyClassAd;
}


PyObject *
py_new_datetime_datetime(long secs) {
	static PyObject * py_datetime_module = NULL;
	if( py_datetime_module == NULL ) {
		 py_datetime_module = PyImport_ImportModule( "datetime" );
	}

	static PyObject * py_datetime_class = NULL;
	if( py_datetime_class == NULL ) {
		py_datetime_class = PyObject_GetAttrString( py_datetime_module, "datetime" );
	}

	return PyObject_CallMethodObjArgs(
	    py_datetime_class,
	    PyUnicode_FromString("fromtimestamp"),
	    PyLong_FromLong(secs)
	);
}


PyObject *
py_new_classad_value( classad::Value::ValueType vt ) {
	static PyObject * py_htcondor_module = NULL;
	if( py_htcondor_module == NULL ) {
		 py_htcondor_module = PyImport_ImportModule( "htcondor" );
	}

	static PyObject * py_htcondor_classad_module = NULL;
	if( py_htcondor_classad_module == NULL ) {
		py_htcondor_classad_module = PyObject_GetAttrString( py_htcondor_module, "classad" );
	}

	static PyObject * py_value_class = NULL;
	if( py_value_class == NULL ) {
		py_value_class = PyObject_GetAttrString( py_htcondor_classad_module, "Value" );
	}

	return PyObject_CallObject(py_value_class, PyLong_FromLong(vt));
}
