static PyObject *
_classad_version( PyObject *, PyObject * ) {
    std::string version;
    classad::ClassAdLibraryVersion( version );
    return PyUnicode_FromString( version.c_str() );
}


static PyObject *
_classad_last_error( PyObject *, PyObject * ) {
    return PyUnicode_FromString( classad::CondorErrMsg.c_str() );
}
