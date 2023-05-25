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
    handle->f = [](void * & v){ dprintf( D_ALWAYS, "[ClassAd]\n" ); delete (classad::ClassAd *)v; v = NULL; };
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
                        // This was ClassAdEvaluationError in version 1.
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
            // This was ClassAdEnumError in version 1.
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
            // This was ClassAdEvaluationError in version 1.
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

    int check = py_is_classad_exprtree(py_v);
    if( check == -1 ) { return NULL; }
    if( check == 1 ) {
        // `py_v` is a htcondor.ClassAd.ExprTree that owns it own ExprTree *,
        // so we have to return a copy here.  We don't have to deparent
        // the ExprTree, because it's already loose (by invariant).
        auto * handle = get_handle_from(py_v);
        return ((classad::ExprTree *)handle->t)->Copy();
    }

    classad::Value v;

    // We can't convert classad.Value to a native Python type in Python
    // because there's no native Python type for classad.Value.Error.
    check = py_is_classad_value(py_v);
    if( check == -1 ) { return NULL; }
    if( check == 1 ) {
        PyObject * py_v_int = PyNumber_Long(py_v);
        if( py_v_int == NULL ) {
            // This was ClassAdInternalError in version 1.
            PyErr_SetString( PyExc_RuntimeError, "Unknown ClassAd value type." );
            return NULL;
        }
        long lv = PyLong_AsLong(py_v_int);
        Py_DecRef(py_v_int);

        if( lv == 1<<0 ) {
            v.SetErrorValue();
            return classad::Literal::MakeLiteral(v);
        } else if( lv == 1<<1 ) {
            v.SetUndefinedValue();
            return classad::Literal::MakeLiteral(v);
        } else {
            // This was ClassAdInternalError in version 1.
            PyErr_SetString( PyExc_RuntimeError, "Unknown ClassAd value type." );
            return NULL;
        }
    }

    if( py_is_datetime_datetime(py_v) ) {


        // We do this song-and-dance in a bunch of places...
        static PyObject * py_htcondor_module = NULL;
        if( py_htcondor_module == NULL ) {
             py_htcondor_module = PyImport_ImportModule( "htcondor2" );
        }

        static PyObject * py_htcondor_classad_module = NULL;
        if( py_htcondor_classad_module == NULL ) {
            py_htcondor_classad_module = PyObject_GetAttrString( py_htcondor_module, "classad" );
        }


        PyObject * py_ts = PyObject_CallMethod(
            py_htcondor_classad_module,
            "_convert_local_datetime_to_utc_ts",
            "O", py_v
        );
        if( py_ts == NULL ) {
            // PyObject_CallMethod() has already set an exception for us.
            return NULL;
        }


        classad::abstime_t atime;
        atime.offset = 0;
        atime.secs = PyLong_AsLong(py_ts);
        if( PyErr_Occurred() ) {
            // PyLong_AsLong() has already set an exception for us.
            return NULL;
        }

        v.SetAbsoluteTimeValue(atime);
        return classad::Literal::MakeLiteral(v);
    }

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

    // This was ClassAdValueError in version 1.
    PyErr_SetString( PyExc_RuntimeError, "Unable to convert Python object to a ClassAd expression." );
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
