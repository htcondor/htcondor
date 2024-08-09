static PyObject *
_param__getitem__( PyObject *, PyObject * args ) {
    const char * key = NULL;

    if(! PyArg_ParseTuple( args, "z", & key )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    // The point of all this nonsense -- instead of just looking the
    // parameter up and returning the string value -- is not to return
    // the equivalent Python type when we know it, but to force the
    // type-associated evaluation to occur.  For instance, boolean
    // parameters will return as True or False if set to 1 or 0.

    std::string dummy;
    const char * pdef_value;
    const MACRO_META * pmeta;
    const char * raw_value = param_get_info( key,
        NULL, NULL, dummy, & pdef_value, & pmeta
    );
    if( raw_value == NULL ) {
        PyErr_SetString(PyExc_KeyError, key);
        return NULL;
    }

    switch( param_default_type_by_id(pmeta->param_id) ) {
        case PARAM_TYPE_STRING: {
            std::string value;
            param(value, key);
            return PyUnicode_FromString(value.c_str());
        } break;

        case PARAM_TYPE_INT: {
            int value = param_integer(key);
            return PyLong_FromLongLong(value);
        } break;

        case PARAM_TYPE_BOOL: {
            bool value = param_boolean(key, false);
            return PyBool_FromLong(value);
        } break;

        case PARAM_TYPE_DOUBLE: {
            double value = param_double(key);
            return PyFloat_FromDouble(value);
        } break;

        case PARAM_TYPE_LONG: {
            long long value = param_integer(key);
            return PyLong_FromLongLong(value);
        } break;

        default: {
            return PyUnicode_FromString(raw_value);
        } break;
    }

    // The compiler is confused.
    return NULL;
}


static PyObject *
_param__setitem__( PyObject *, PyObject * args ) {
    const char * key = NULL;
    const char * value = NULL;

    if(! PyArg_ParseTuple( args, "zz", & key, & value )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    // It's implied in condor_config.h that this isn't general-purpose...
    param_insert(key, value);


    Py_RETURN_NONE;
}


static PyObject *
_param__delitem__( PyObject *, PyObject * args ) {
    const char * key = NULL;

    if(! PyArg_ParseTuple( args, "z", & key )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    // It's implied in condor_config.h that this isn't general-purpose...
    param_insert(key, "");


    Py_RETURN_NONE;
}


bool record_keys( void * k, HASHITER & it ) {
    auto keys = (std::vector<std::string> *)k;

    const char * key = hash_iter_key(it);

    // If we use hash_iter_value(), we get the raw value, which is wrong
    // (and inconsistent) because we return the cooked value, and if the
    // raw value was entirely macro substitution, the cooked value could
    // be the empty string -- which we pretend means the key is missing.
    std::string value;
    if( param( value, key ) ) {
        keys->push_back(key);
    }

    return true;
}


static PyObject *
_param_keys( PyObject *, PyObject * args ) {
    if(! PyArg_ParseTuple( args, "" )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    std::vector<std::string> keys;
    foreach_param(0, & record_keys, (void *)&keys);

    size_t bufferSize = 0;
    for( const auto & key : keys ) { bufferSize += 1 + key.size(); }

    size_t pos = 0;
    std::string buffer;
    buffer.resize(bufferSize);
    for( const auto & key : keys ) {
        buffer.replace(pos, key.size(), key);
        buffer[pos + key.size()] = '\0';
        pos += key.size() + 1;
    }


    return PyUnicode_FromStringAndSize( buffer.c_str(), buffer.size() - 1 );
}
