//
// We can't statically define Python types from C if Py_LIMITED_API is set.
//
// See python-bindings.md for some more-detailed ranting about this file.
//
//

static PyObject * _handle_new(
    PyTypeObject * type, PyObject * args, PyObject * kwds
);
static void _handle_dealloc(PyObject * self);


template<class T>
struct DynamicPyType {
    DynamicPyType(const char * module_dot_typename) {
        type_spec.name = module_dot_typename;
    }

    typedef struct {
        // All Python objects begin within PyObject_HEAD (a macro; no semicolon
        // is required or some compilers, permitted); it defines the fields the
        // Python interpreter uses for its own purposes.
        //
        // For more details, see https://docs.python.org/3/extending/newtypes_tutorial.html#the-basics
        PyObject_HEAD
        // Although the compiler would allow us to specify initial values
        // for these fields, there's no mechanism by which Python could
        // know about them when it allocates the object, so let's not tempt
        // people to make bad assumptions.
        T t;
        void (* f)(T & t);
    } python_object_type;

    PyType_Spec type_spec = {
        .name = NULL, /* set by constructor */
        .basicsize = sizeof(python_object_type),
        .itemsize = 0, /* not a variable-sized object */
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE,
        .slots = dynamic_slots,
    };

    PyType_Slot dynamic_slots[3] = {
        {Py_tp_new, (void *) & _handle_new},
        {Py_tp_dealloc, (void *) & _handle_dealloc},
        {0, NULL},
    };
};


typedef DynamicPyType<void *> DynamicPyType_Handle;
typedef DynamicPyType_Handle::python_object_type PyObject_Handle;


static PyObject *
_handle_new( PyTypeObject * type, PyObject * args, PyObject * kwargs ) {
       PyObject * self = PyType_GenericNew(type, args, kwargs);
       if( self != NULL ) {
               PyObject_Handle * h = (PyObject_Handle *)self;
               h->t = NULL;
               h->f = NULL;
       }
       return self;
}


static void
_handle_dealloc(PyObject * self) {
    auto * handle = (PyObject_Handle *)self;
    dprintf( D_PERF_TRACE, "_handle_dealloc(%p)\n", handle->t );
    PyTypeObject * tp = Py_TYPE(self);

    if(handle->f == NULL) {
        if(handle->t != NULL) {
            dprintf( D_ALWAYS, "_handle_dealloc(%p) has no registered callback\n", handle->t );
        } else {
            dprintf( D_PERF_TRACE, "_handle_dealloc(%p) has no registered callback\n", handle->t );
        }
    } else {
        handle->f(handle->t);
    }

    // The examples all call `tp->tp_free(self)`, but of course we
    // can't do that because we set Py_LIMITED_API.  However, if we
    // don't set Py_TPFLAGS_BASETYPE, we can call the deallocator
    // directly.  Since Py_TPFLAGS_HAVE_GC isn't set, that's going
    // to be PyObject_Del().  See the following URL:
    //
    // https://docs.python.org/3/c-api/typeobj.html?highlight=py_tpflags_basetype#c.PyTypeObject.tp_dealloc
    PyObject_Del(self);

    // Required because DynamicPyType<> sets Py_TPFLAGS_HEAPTYPE.
    Py_DECREF(tp);
}
