static PyObject *
_classad_init( PyObject *, PyObject * args ) {
    // _classad_init( self, self._handle )

    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    handle->t = new classad::ClassAd();
    handle->f = [](void * & v){ delete (classad::ClassAd *)v; v = NULL; };
    Py_RETURN_NONE;
}


static PyObject *
_classad_to_string( PyObject *, PyObject * args ) {
    // _classad_to_string( self._handle )

    PyObject_Handle * handle = NULL;
    if(! PyArg_ParseTuple( args, "O", (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::string str;
    classad::PrettyPrint pp;
    pp.Unparse( str, (ClassAd *)handle->t );

    return PyUnicode_FromString(str.c_str());
}


static PyObject *
_classad_to_repr( PyObject *, PyObject * args ) {
    // _classad_to_string( self._handle )

    PyObject_Handle * handle = NULL;
    if(! PyArg_ParseTuple( args, "O", (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::string str;
    classad::ClassAdUnParser up;
    up.Unparse( str, (ClassAd *)handle->t );

    return PyUnicode_FromString(str.c_str());
}


bool
should_convert_to_python(classad::ExprTree * e) {
    auto kind = e->GetKind();

    if( kind == classad::ExprTree::EXPR_ENVELOPE ) {
        classad::CachedExprEnvelope * inner =
          static_cast<classad::CachedExprEnvelope*>(e);
        kind = inner->get()->GetKind();
    }

    switch( kind ) {
        case classad::ExprTree::LITERAL_NODE:
        case classad::ExprTree::CLASSAD_NODE:
        case classad::ExprTree::EXPR_LIST_NODE:
            return true;
        default:
            return false;
    }
}


PyObject *
convert_classad_value_to_python(classad::Value & v) {
    switch( v.GetType() ) {
        case classad::Value::CLASSAD_VALUE:
        case classad::Value::SCLASSAD_VALUE: {
            ClassAd * c;
            (void) v.IsClassAdValue(c);

            return py_new_classad_classad(c->Copy());
            } break;

        case classad::Value::BOOLEAN_VALUE: {
            bool b;
            (void) v.IsBooleanValue(b);

            if(b) { Py_RETURN_TRUE; } else { Py_RETURN_FALSE; }
            } break;

        case classad::Value::STRING_VALUE: {
            std::string s;
            (void) v.IsStringValue(s);
            return PyUnicode_FromString(s.c_str());
            } break;

        case classad::Value::ABSOLUTE_TIME_VALUE: {
            classad::abstime_t atime; atime.secs=0; atime.offset=0;
            (void) v.IsAbsoluteTimeValue(atime);

            // An absolute time value is always in UTC.
            return py_new_datetime_datetime(atime.secs);
            } break;

        case classad::Value::RELATIVE_TIME_VALUE: {
            double d;
            (void) v.IsRelativeTimeValue(d);
            return PyFloat_FromDouble(d);
            } break;

        case classad::Value::INTEGER_VALUE: {
            long long ll;
            (void) v.IsIntegerValue(ll);
            return PyLong_FromLongLong(ll);
            } break;

        case classad::Value::REAL_VALUE: {
            double d;
            (void) v.IsRealValue(d);
            return PyFloat_FromDouble(d);
            } break;

        case classad::Value::ERROR_VALUE: {
            return py_new_classad_value(classad::Value::ERROR_VALUE);
            } break;

        case classad::Value::UNDEFINED_VALUE: {
            return py_new_classad_value(classad::Value::UNDEFINED_VALUE);
            } break;

        case classad::Value::LIST_VALUE:
        case classad::Value::SLIST_VALUE: {
            classad_shared_ptr<classad::ExprList> l;
            (void) v.IsSListValue(l);

            PyObject * list = PyList_New(0);
            if( list == NULL ) {
                PyErr_SetString( PyExc_MemoryError, "convert_classad_value_to_python" );
                return NULL;
            }

            for( auto i = l->begin(); i != l->end(); ++i ) {
                if( should_convert_to_python(*i) ) {
                    classad::Value v;
                    if(! (*i)->Evaluate( v )) {
                        // FIXME: ClassAdEvaluationError
                        PyErr_SetString( PyExc_RuntimeError, "Failed to evaluate convertible expression" );
                        return NULL;
                    }

                    PyObject * py_v = convert_classad_value_to_python(v);
                    if(PyList_Append( list, py_v ) != 0) {
                        // PyList_Append() has already set an exception for us.
                        return NULL;
                    }
                } else {
                    return py_new_classad_exprtree( *i );
                }
            }

            return list;
            } break;

        default:
            // FIXME: ClassAdEnumError
            PyErr_SetString( PyExc_RuntimeError, "Unknown ClassAd value type" );
            return NULL;
    }
}


static PyObject *
_classad_get_item( PyObject *, PyObject * args ) {
    // _classad_get_item( self._handle, key )

    PyObject_Handle * handle = NULL;
    const char * key = NULL;
    if(! PyArg_ParseTuple( args, "Os", (PyObject **)& handle, & key )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto * classAd = (ClassAd *)handle->t;
    classad::ExprTree * expr = classAd->Lookup( key );
    if(! expr) {
        PyErr_SetString( PyExc_KeyError, key );
        return NULL;
    }


    if( should_convert_to_python( expr ) ) {
        classad::Value v;
        if(! expr->Evaluate( v )) {
            // FIXME: ClassAdEvaluationError
            PyErr_SetString( PyExc_RuntimeError, "Failed to evaluate convertible expression" );
            return NULL;
        }

        return convert_classad_value_to_python(v);
    }


    return py_new_classad_exprtree( expr );
}


ExprTree *
convert_python_to_classad_exprtree(PyObject * py_v) {
    if( py_v == Py_None ) {
        return classad::Literal::MakeUndefined();
    }

    // FIXME: Is py_v a ClassAd.ExprTree?

    // FIXME: Is py_v a ClassAd.Value?

    // FIXME: Is py_v a DateTime object?

    classad::Value v;

    if( PyBool_Check(py_v) ) {
        v.SetBooleanValue( py_v == Py_True );
        return classad::Literal::MakeLiteral(v);
    }

    if( PyUnicode_Check(py_v) ) {
        PyObject * py_bytes = PyUnicode_AsUTF8String(py_v);

        char * buffer = NULL;
        Py_ssize_t size = -1;
        if( PyBytes_AsStringAndSize(py_bytes, &buffer, &size) == -1 ) {
            // PyBytes_AsStringAndSize() has already set an exception for us.
            return NULL;
        }
        v.SetStringValue( buffer, size );

        Py_DECREF(py_bytes);
        return classad::Literal::MakeLiteral(v);
    }

    if( PyBytes_Check(py_v) ) {
        char * buffer = NULL;
        Py_ssize_t size = -1;
        if( PyBytes_AsStringAndSize(py_v, &buffer, &size) == -1 ) {
            // PyBytes_AsStringAndSize() has already set an exception for us.
            return NULL;
        }
        v.SetStringValue( buffer, size );
        return classad::Literal::MakeLiteral(v);
    }

    if( PyLong_Check(py_v) ) {
        // This should almost certainly be ...AsLongLong(), but the
        // error is in the original version and in the classad3 module,
        // this function won't exist anyway.
        v.SetIntegerValue(PyLong_AsLong(py_v));
        return classad::Literal::MakeLiteral(v);
    }

    if( PyFloat_Check(py_v) ) {
        v.SetRealValue(PyFloat_AsDouble(py_v));
        return classad::Literal::MakeLiteral(v);
    }

    // FIXME: PyDict_Check()

    // FIXME: PyMapping_Check()

    // FIXME: PyObject_GetIter()

    // FIXME: ClassAdEnumError
    PyErr_SetString( PyExc_RuntimeError, "Unknown ClassAd value type" );
    return NULL;
}

static PyObject *
_classad_set_item( PyObject *, PyObject * args ) {
    // _classad_set_item( self._handle, key, value )

    PyObject_Handle * handle = NULL;
    const char * key = NULL;
    PyObject * value = NULL;
    if(! PyArg_ParseTuple( args, "OsO", (PyObject **)& handle, & key, & value )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto * classAd = (ClassAd *)handle->t;
    ExprTree * v = convert_python_to_classad_exprtree(value);
    if(! classAd->Insert(key, v)) {
        PyErr_SetString(PyExc_AttributeError, key);
        return NULL;
    }

    Py_RETURN_NONE;
}
