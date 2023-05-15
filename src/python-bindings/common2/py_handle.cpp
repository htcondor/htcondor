//
// We can't statically define Python types from C if Py_LIMITED_API is set.
//
// See python-bindings.md for some more-detailed ranting about this file.
//
//

static void _handle_dealloc(PyObject * self);


template<class T>
struct DynamicPyType {
    DynamicPyType(const char * module_dot_typename) {
        type_spec.name = module_dot_typename;
    }

    typedef struct {
        PyObject_HEAD
        T t;
    } python_object_type;

    PyType_Spec type_spec = {
        .name = NULL, /* set by constructor */
        .basicsize = sizeof(python_object_type),
        .itemsize = 0, /* not a variable-sized object */
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE,
        .slots = dynamic_slots,
    };

    PyType_Slot dynamic_slots[3] = {
        {Py_tp_dealloc, (void *) & _handle_dealloc},
        {0, NULL},
    };
};


typedef DynamicPyType<void *> DynamicPyType_Handle;
typedef DynamicPyType_Handle::python_object_type PyObject_Handle;


static void
_handle_dealloc(PyObject * self) {
    auto * handle = (PyObject_Handle *)self;
    dprintf( D_ALWAYS, "_handle_dealloc(%p)\n", handle->t );
    PyTypeObject * tp = Py_TYPE(self);

    // FIXME: delete something.

    // The examples all call `tp->tp_free(self)`, but of course we
    // can't do that because we set Py_LIMITED_API.  However, if we
    // don't set Py_TPFLAGS_BASETYPE, we can call the deallocator
    // directly.  Since Py_TPFLAGS_HAVE_GC isn't set, that's going
    // to be PyObject_Del().
    PyObject_Del(self);

    // Required because DynamicPyType<> sets Py_TPFLAGS_HEAPTYPE.
    Py_DECREF(tp);
}
