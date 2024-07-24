condor_exe( condor_gpu_utilization
	"condor_gpu_utilization.cpp;cuda_device_enumeration.cpp;print_error.cpp"
	${C_LIBEXEC} "${CMAKE_DL_LIBS}" OFF )

condor_exe( condor_gpu_discovery
    "condor_gpu_discovery.cpp;cuda_device_enumeration.cpp;opencl_device_enumeration.cpp;hip_device_enumeration.cpp;simulated_device_enumeration.cpp;print_error.cpp"
    ${C_LIBEXEC} "${CMAKE_DL_LIBS}" OFF )
