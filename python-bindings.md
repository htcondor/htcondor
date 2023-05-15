- For some awful reason, the Python parts of the implementations are in
  bindings/python/, but the C/C++ parts in src/python-bindings.

- The `htcondor` and `classad` modules each have two versions: the original
  version that depends on Boost and the `htcondor2` and `classad2` versions
  that strive to be bug-for-bug compatible but use only the Python C API.

- The `classad3` module is intended to be API breaking but a major leap
  forward in terms of implementation simplicity -- and therefore
  maintainabililty -- as well as a substantial improvement to the API.
  It is intended that there be a corresponding `htcondor3` module.

- The `htcondor2` and `classad2` modules are currently implemented as
  Python monkey-patches on top of the `htcondor` and `classad` modules;
  they will eventually become the only implementation, although we
  may not ever actually rename them even after the last of the original
  implementations are swept away.

- For classad2._class_ad, the invariant is that handle->t is always pointing
  to an owned (and valid) ClassAd on the heap.  When Python deallocates a
  classad2._class_ad, (FIXME) the C deallocation function will delete the
  pointer.  The pointer will never otherwise be deleted.

- The htcondor._collector class has identical semantics for its handle.
  (Note: we should probably move the htcondor2 python files to their own
  module directory and do only the monkey-patching in the htcondor/ dir.)
