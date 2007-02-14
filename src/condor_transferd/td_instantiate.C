#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"
#include "list.h"
#include "condor_td.h"

template class SimpleList<ClassAd*>;
template class HashTable<MyString, TransferRequest*>;
template class HashTable<long, TransferRequest*>;
