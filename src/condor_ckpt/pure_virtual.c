
/**
Pure virtual functions elsewhere in Condor cause references
to a g++ symbol, __pure_virtual.  When the c++ library is
not present, i.e. linking with a fortran program, we need
to provide a reference for this symbol.
*/

void __pure_virtual()
{
}

