condor_exe( condor_gpu_utilization
	"cuda_device_enumeration.cpp;condor_gpu_utilization.cpp"
	${C_LIBEXEC} "${CMAKE_DL_LIBS}" OFF )

condor_exe( condor_gpu_discovery
    "condor_gpu_discovery.cpp"
    ${C_LIBEXEC} "${CMAKE_DL_LIBS}" OFF )
