#ifndef   _BASICPROPS_H
#define   _BASICPROPS_H


class BasicProps {
	public:
		BasicProps();

		std::string   uuid;
		std::string   name;
		char          pciId[32];
		size_t        totalGlobalMem {(size_t)-1};
		int           ccMajor {-1};
		int           ccMinor {-1};
		int           multiProcessorCount {-1};
		int           clockRate {-1};
		int           ECCEnabled {-1};
		int	      xNACK {-1};
		int	      warpSize {-1};
		int	      driverVersion {-1};

		void setUUIDFromBuffer( const unsigned char buffer[16] );
};
#endif
