include_directories( ${CONDOR_SOURCE_DIR}/src/condor_tools )
condor_exe( condor_gpu_utilization
	"cuda_device_enumeration.cpp;condor_gpu_utilization.cpp"
	${C_LIBEXEC} "${CMAKE_DL_LIBS}" OFF )
