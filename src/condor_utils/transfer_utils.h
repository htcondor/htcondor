#ifndef   _CONDOR_TRANSFER_UTILS_H
#define   _CONDOR_TRANSFER_UTILS_H

// #include <vector>
// #include <string>
// #include <filesystem>

std::vector<std::tuple< std::filesystem::path, bool, size_t >>
check_paths(
    const std::vector<std::filesystem::path> & paths,
    const std::filesystem::path & iwd;
);

#endif /* _CONDOR_TRANSFER_UTILS_H */
