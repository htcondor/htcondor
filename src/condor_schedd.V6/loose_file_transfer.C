#include "condor_common.h"
#include "string_list.h"
#include "file_transfer.h"

#include "loose_file_transfer.h"

LooseFileTransfer::LooseFileTransfer()
{
  FileTransfer();
};

bool
LooseFileTransfer::getInputFiles(StringList & inputFileList)
{
  char *buffer;

  InputFiles->rewind();
  while (NULL != (buffer = InputFiles->next())) {
    inputFileList.append(buffer);
  }

  return true;
};
