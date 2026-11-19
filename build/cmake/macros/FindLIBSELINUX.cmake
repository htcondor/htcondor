# Find and export libselinux (optional; used by the startd's VolumeManager
# to label per-job LVM filesystems correctly under SELinux).

include(FindPackageHandleStandardArgs)

find_library(LIBSELINUX_LIBRARY NAMES selinux)
find_path(LIBSELINUX_INCLUDE_DIR NAMES selinux/selinux.h)

find_package_handle_standard_args(LIBSELINUX REQUIRED_VARS LIBSELINUX_LIBRARY LIBSELINUX_INCLUDE_DIR)

if (LIBSELINUX_FOUND)
	mark_as_advanced(LIBSELINUX_INCLUDE_DIR)
	mark_as_advanced(LIBSELINUX_LIBRARY)
endif()

if (LIBSELINUX_FOUND AND NOT TARGET LIBSELINUX::LIBSELINUX)
	add_library(LIBSELINUX::LIBSELINUX SHARED IMPORTED)
	set_property(TARGET LIBSELINUX::LIBSELINUX PROPERTY IMPORTED_LOCATION ${LIBSELINUX_LIBRARY})
	target_include_directories(LIBSELINUX::LIBSELINUX INTERFACE ${LIBSELINUX_INCLUDE_DIR})
endif()
