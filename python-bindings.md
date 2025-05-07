- For some awful reason, the Python parts of the implementations are in
  bindings/python/, but the C/C++ parts in src/python-bindings/.

- The `htcondor` and `classad` modules each have two versions: the original
  version that depends on Boost and the `htcondor2` and `classad2` versions
  that strive to be bug-for-bug compatible but use only the Python C API.

- When writing docstrings, be aware that the `htcondor` and `classad`
  module documentation *does not* automatically pick up new docstrings.
  The `htcondor2` and `classad2` documentation is written so that new
  methods are generally picked up automatically, but new classes are
  not.

- The `classad3` module is intended to be API breaking but a major leap
  forward in terms of implementation simplicity -- and therefore
  maintainabililty -- as well as a substantial improvement to the API.
  It is intended that there be a corresponding `htcondor3` module.

- The `htcondor2` and `classad2` modules should be drop-in replacements
  for their predecessors: `import htcondor2 as htcondor` and
  `import classad2 as classad`.  For various reasons, including necessary
  deprecations, that's not 100% true.  For this reason, the version of
  the test suite that runs using those two modules is kept in a separate
  branch.

- For classad2._class_ad, the invariant is that handle->t is always pointing
  to an owned (and valid) ClassAd on the heap.  When Python deallocates a
  classad2._class_ad, the deallocation function (handle->f) will delete the
  pointer.  The pointer will never otherwise be deleted.

- The htcondor._collector class has identical semantics for its handle.
  (Note: we should probably move the htcondor2 python files to their own
  module directory and do only the monkey-patching in the htcondor/ dir.)

- Because we're setting Py_LIMITED_API, we have to construct our types
  "dynamically" (meaning, we can't just declare new Python type objects
  because the internals change between minor versions).  This is annoying,
  because the tutorial assumes static types; new types were already painful
  enough to introduce.  So the "handle" type is a bare-bones as possible.

- Python-side objects which need to keep a C/C++ pointer alive should have
  the following lines in their __init__() function:

    self._handle = handle_t()
    _<typename>_init(self, self._handle)

  so that the C/C++ _<typename>_init() function can easily store the
  proper pointer.  (You can add extra arguments if you want.)

- Use the functions in py_util to deal with handles.  You MUST NOT
  increase the reference count on a handle from C/C++.
