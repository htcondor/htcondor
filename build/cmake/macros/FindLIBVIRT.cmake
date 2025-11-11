# Cmake doesn't provide this, so we write our own, following the cmake structure
# Find and export libvirt

# cmake helpers
include(FindPackageHandleStandardArgs)

find_library(LIBVIRT_LIBRARY NAMES virt REQUIRED)
find_path(LIBVIRT_INCLUDE_DIR NAMES libvirt/libvirt.h REQUIRED)

find_package_handle_standard_args(LIBVIRT REQUIRED_VARS LIBVIRT_INCLUDE_DIR HANDLE_COMPONENTS)

if (LIBVIRT_FOUND)
	mark_as_advanced(LIBVIRT_INCLUDE_DIR)
	mark_as_advanced(LIBVIRT_LIBRARY)
endif()

if (LIBVIRT_FOUND AND NOT TARGET LIBVIRT::LIBVIRT)
	add_library(LIBVIRT::LIBVIRT SHARED IMPORTED)
	set_property(TARGET LIBVIRT::LIBVIRT PROPERTY IMPORTED_LOCATION ${LIBVIRT_LIBRARY})
	target_include_directories(LIBVIRT::LIBVIRT INTERFACE ${LIBVIRT_INCLUDE_DIR})
endif()
