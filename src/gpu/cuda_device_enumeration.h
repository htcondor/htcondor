#ifndef   _CUDA_DEVICE_ENUMERATION_H
#define   _CUDA_DEVICE_ENUMERATION_H

// basic device properties we can query from the driver
class BasicProps {
	public:
		BasicProps();

		std::string   name;
		unsigned char uuid[16];
		char          pciId[16];
		size_t        totalGlobalMem;
		int           ccMajor;
		int           ccMinor;
		int           multiProcessorCount;
		int           clockRate;
		int           ECCEnabled;

		bool hasUUID() { return uuid[0] || uuid[6] || uuid[8]; }
		const char * printUUID();
};

bool enumerateCUDADevices( std::vector< BasicProps > & devices );

#endif /* _CUDA_DEVICE_ENUMERATION_H */
