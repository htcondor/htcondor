static PyObject *
_exprtree_init(PyObject *, PyObject * args) {
	// _exprtree_init( self, self._handle )

    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    // We don't know which kind of ExprTree this is going to be yet,
    // so we can't initialize it to anything (ExprTree proper is abstract).
    handle->t = NULL;
    handle->f = [](void *& v) { dprintf( D_PERF_TRACE, "[unconstructed ExprTree]\n" ); if( v != NULL ) { dprintf(D_ALWAYS, "Error!  Unconstructed ExprTree has non-NULL handle %p\n", v); } };
    Py_RETURN_NONE;
}


static PyObject *
_exprtree_eq( PyObject *, PyObject * args ) {
    // _exprtree_eq( self._handle, other._handle )

    PyObject_Handle * self = NULL;
    PyObject_Handle * other = NULL;
    if(! PyArg_ParseTuple( args, "OO", (PyObject **)& self, (PyObject **)& other)) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    if( *(classad::ExprTree *)self->t == *(classad::ExprTree *)other->t ) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}


bool
EvaluateLooseExpr( classad::ExprTree * expr,
                   classad::ClassAd * my, classad::ClassAd * target,
                   classad::Value & value ) {
    const classad::ClassAd * originalScope = expr->GetParentScope();
    expr->SetParentScope(my);

    bool rv = false;
    if( target == my || target == NULL ) {
        rv = expr->Evaluate( value );
    } else {
        classad::MatchClassAd matchAd( my, target );
        rv = expr->Evaluate( value );
        matchAd.RemoveLeftAd();
        matchAd.RemoveRightAd();
    }

    expr->SetParentScope(originalScope);
    return rv;
}


bool
evaluate( classad::ExprTree * expr, classad::ClassAd * scope,
  classad::ClassAd * target, classad::Value & value
) {
    if( scope != NULL ) {
        return EvaluateLooseExpr( expr, scope, target, value );
    } else if( expr->GetParentScope() ) {
        return expr->Evaluate( value );
    } else {
        classad::EvalState state;
        return expr->Evaluate( state, value );
    }
}



static PyObject *
_exprtree_eval( PyObject *, PyObject * args ) {
    // _exprtree_eval( self._handle, scope )

    PyObject_Handle * self = NULL;
    PyObject_Handle * scope = NULL;
    PyObject_Handle * target = NULL;
    if(! PyArg_ParseTuple( args, "OOO", (PyObject **)& self, (PyObject **)& scope, (PyObject **)& target)) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    auto e = (classad::ExprTree *)self->t;
    classad::ClassAd * s = NULL;
    if( scope != NULL && scope != (PyObject_Handle *)Py_None ) {
        s = (classad::ClassAd *)scope->t;
    }

    classad::ClassAd * t = NULL;
    if( target != NULL && target != (PyObject_Handle *)Py_None ) {
        t = (classad::ClassAd *)target->t;
    }


    classad::Value v;
    if(! evaluate( e, s, t, v )) {
        // In version 1, this was ClassAdEvaluationError.
        PyErr_SetString(PyExc_RuntimeError, "failed to evaluate expression");
        return NULL;
    }

    return convert_classad_value_to_python(v);
}


static PyObject *
_exprtree_simplify( PyObject *, PyObject * args ) {
    // _exprtree_eval( self._handle, scope )

    PyObject_Handle * self = NULL;
    PyObject_Handle * scope = NULL;
    PyObject_Handle * target = NULL;
    if(! PyArg_ParseTuple( args, "OOO", (PyObject **)& self, (PyObject **)& scope, (PyObject **)& target)) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    auto e = (classad::ExprTree *)self->t;

    classad::ClassAd * s = NULL;
    if( scope != NULL && scope != (PyObject_Handle *)Py_None ) {
        s = (classad::ClassAd *)scope->t;
    }

    classad::ClassAd * t = NULL;
    if( target != NULL && target != (PyObject_Handle *)Py_None ) {
        t = (classad::ClassAd *)target->t;
    }


    // This construction is confusing, but MakeUndefined() returns a new
    // Literal and overwrites `v` with the address of a member of the return;
    // we thus don't have to free/delete `v`.
    //
    // Not sure why this has to be so hard, but let's not mess with it.
    classad::Value v;
    v.SetUndefinedValue();
    if(! evaluate( e, s, t, v )) {
        // In version 1, this was ClassAdEvaluationError.
        PyErr_SetString(PyExc_RuntimeError, "failed to evaluate expression");
        return NULL;
    }

    // It seems perfectly legitimate to have a literal list or literal
    // ClassAd, but that's not, apparently, how we do things around here.
    PyObject * rv = NULL;
    switch( v.GetType() ) {
        case classad::Value::LIST_VALUE:
        case classad::Value::SLIST_VALUE: {
            classad::ExprList * list = NULL;
            // We don't care who owns the list because we're copying
            // it before `v` goes out of scope.
            ASSERT(v.IsListValue(list));

            rv = py_new_classad_exprtree(list);
            } break;

        case classad::Value::CLASSAD_VALUE:
        case classad::Value::SCLASSAD_VALUE: {
            classad::ClassAd * classAd = NULL;
            // We don't care who owns the ClassAd because we're
            // copying it before `v` goes out of scope.
            ASSERT(v.IsClassAdValue(classAd));

            rv = py_new_classad_exprtree(classAd);
            } break;

        default:
            classad::Literal * literal = classad::Literal::MakeLiteral(v);
            rv = py_new_classad_exprtree(literal);
            break;
    }

    delete literal;
    return rv;
}
