#if defined(BUILD_HELPER)
#include "condor_common.h"
#include <cassert>
#include <string>
#include <fstream>
#include "helper.h"
#include "condor_config.h"

int main(int argc, char* argv[])
{
  assert(argc == 2);

  config();   // load Condor configuration info

  std::string output_file(Helper().resolve(argv[1]));
  std::ofstream output_file_created(output_file.c_str());
  assert(output_file_created);
}
#else
int main() { return 0; }
#endif
