include_directories( ${CONDOR_SOURCE_DIR}/src/condor_tools )
condor_exe( condor_gpu_utilization
	"condor_gpu_utilization.cpp;gpu_function_pointers.cpp;cuda_device_enumeration.cpp"
	${C_LIBEXEC} "${HAVE_LIBDL}" OFF )
