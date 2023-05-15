//
// The implementation functions will need access to the `dynamic_pytype`
// struct declaration.
//

//
// We can't statically define Python types from C if Py_LIMITED_API is set,
// which we're not quite ready to give up on yet.  Also, it's a pain to
// define even a single Python type, so the plan was to define that single
// Python type to have a C-side-only void * for whatever nefarious purpose
// the C-side needs, and have the interface types inherit from that type in
// pure Python.

// For now, we're going to try to "dynamically" define that type.  If we
// can't make that work, given that we're declaring a void * here, maybe
// it wouldn't be so bad, or even better, just to declare ints in Python
// that only the C side uses for pointers.

// So the following seems to more-or-less work as a way to more-or-less
// easily add a Python type that has-a more-or-less arbitrary singular
// (by-value) type.  Note that the value's storage space will be
// deallocated but the destructor, if any, won't be called.

// In some cases, we may not, effectively, be able to use value-types
// and have to use reference types.  However, we don't have to use
// pointers or references -- we could maintain a handle table.  Only
// the C side of the code would ever remove an entry from the handle
// table (which would have to be an owning pointer).  Stale handles
// would raise ValueErrors, and if at some point in the future we
// feel like doing a lot of work, we could extend the handle type
// to (a) be uncopyable and (b) call a C function when the Python
// side runs out of refcounts...

//
// So we can do something with __copy__ and __deepcopy__ to prevent
// copies from being made in the usual way.  It looks like we would
// otherwise only need to set `tp_dealloc` ... which could maybe
// just deal with a shared pointer?
//

//
// Rather than _inherit_ a handle, and have to worry about where the
// memory layout of the subsequent PyObject, just have the classes
// have-a handle and pass it along as an argument.
//


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
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE | Py_TPFLAGS_BASETYPE,
        .slots = dynamic_slots,
    };

    PyType_Slot dynamic_slots[1] = { {0, NULL} };
};


typedef DynamicPyType<void *> DynamicPyType_Handle;
typedef DynamicPyType_Handle::python_object_type PyObject_Handle;
