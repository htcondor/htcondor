#ifndef   _BASICPROPS_H
#define   _BASICPROPS_H


class BasicProps {
	public:
		BasicProps();

		std::string   uuid;
		std::string   name;
		std::string   driver;
		char          pciId[32];
		size_t        totalGlobalMem {(size_t)-1};
		int           ccMajor {-1};
		int           ccMinor {-1};
		int           multiProcessorCount {-1};
		int           clockRate {-1};
		int           ECCEnabled {-1};
		int           integrated {-1};
		int           xNACK {-1};
		int           warpSize {-1};
		int           driverVersion {-1};
		int           hipDetection{0};

		// Only set for NVIDIA MIG instances: the NVML uuid ("GPU-<uuid>") of the
		// physical GPU hosting this instance, and the instance's GPU and compute
		// instance ids.  Together these locate the instance's nvidia-caps access
		// files under /proc/driver/nvidia/capabilities/gpu<minor>/mig/gi<G>/ci<C>
		std::string   parentUuid;
		int           migGpuInstanceId {-1};
		int           migComputeInstanceId {-1};

		void setUUIDFromBuffer( const unsigned char buffer[16] );
};
#endif
