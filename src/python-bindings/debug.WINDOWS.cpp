
// !!!!! WARNING WARNING !!!!!
// This code provides a python27_d.dll  that will be sufficient to get htcondor python bindings to link
// in Debug builds. but they WILL NOT WORK!!  The python object structure is different between _DEBUG
// builds and non-debug builds. To have working python bindings, you must either build RelWithDebugInfo,
// or link with a python27_d.dll that you get by building the python sources, not these little hack functions.
// The use for these hack functions is to get HTCondor to build when you have python installed and you
// don't care if the python bindings work and you don't want to muck with the cmake logic to remove the
// !!!!! WARNING WARNING !!!!!

#if 1
#include "python_bindings_common.h"
#else
// note this module does NOT #include python_bindings_common.h because - unlike the rest of the code -
// it needs to let pyconfig.h see _DEBUG

// pyconfig.h defines PLATFORM in a way that is incompatibable with HTCondor, (theirs is "win32")
// so push and undef the macro before, and restore our definition after we call pyconfig.h
#pragma push_macro("PLATFORM")
#undef PLATFORM
#include <pyconfig.h>
#pragma pop_macro("PLATFORM")
#endif

typedef struct _object PyObject;
typedef ssize_t Py_ssize_t;
typedef void (*destructor)(PyObject *);

extern "C" {

// define these so that we can have debug builds that use python bindings
// even if you dont build python yourself in order to get it to export
// these things.
__declspec(dllexport) Py_ssize_t _Py_RefTotal;

__declspec(dllexport) void _Py_NegativeRefcount(const char *fname, int lineno, PyObject *op) {
	// this should report negative refcounts somehow??
}

__declspec(dllexport) Py_ssize_t _Py_GetRefTotal(void) { return _Py_RefTotal; }

#define HAS_LINKED_LIST_PTRS

__declspec(dllexport) void _Py_Dealloc(PyObject *op) {
	struct _hack_object {
	#ifdef HAS_LINKED_LIST_PTRS
		struct _object *_ob_next;
		struct _object *_ob_prev;
	#endif
		Py_ssize_t ob_refcnt;
		struct {
		#ifdef HAS_LINKED_LIST_PTRS
			struct _object *_ob_next;
			struct _object *_ob_prev;
		#endif
			Py_ssize_t ob_refcnt;
			Py_ssize_t ob_size; /* Number of items in variable part */
			const char *tp_name; /* For printing, in format "<module>.<name>" */
			Py_ssize_t tp_basicsize, tp_itemsize; /* For allocation */

    /* Methods to implement standard operations */
			destructor tp_dealloc;

		} *ob_type;
	} * pop = (struct _hack_object*)op;

	pop->ob_type->tp_dealloc(op);
}

} // extern "C"

