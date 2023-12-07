static PyObject *
_version( PyObject *, PyObject * ) {
    std::string version;
    classad::ClassAdLibraryVersion( version );
    return PyUnicode_FromString( version.c_str() );
}
