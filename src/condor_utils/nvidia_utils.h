#ifndef NVIDIA_UTILS_H
#define NVIDIA_UTILS_H

#ifdef LINUX
#include "sys/sysmacros.h" // for dev_t

#include <string>
#include <vector>

std::vector<std::pair<std::string, dev_t>> make_nvidia_uuid_to_dev_map();
std::vector<dev_t> nvidia_env_var_to_exclude_list(const std::string &visible_devices_env);
#endif
#endif
