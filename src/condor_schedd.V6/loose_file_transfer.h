#include "string_list.h"
#include "file_transfer.h"

class LooseFileTransfer : public FileTransfer
{
public:
  LooseFileTransfer::LooseFileTransfer();

  bool getInputFiles(StringList &);
};
