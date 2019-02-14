#include <string.h>

#include <string>
#include <vector>

#include "nvml_stub.h"
#include "cuda_header_doc.h"
#include "pi_dynlink.h"
#include "gpu_function_pointers.h"
#include "cuda_device_enumeration.h"

bool
enumerateCUDADevices( std::vector< BasicProps > & devices ) {
	int deviceCount = 0;

	if( (cudaGetDeviceCount(&deviceCount) != cudaSuccess) ) {
		return false;
	}

	for( int dev = 0; dev < deviceCount; ++dev ) {
		BasicProps bp;
		if( cudaSuccess == getBasicProps( dev, &bp ) ) {
			devices.push_back(bp);
		}
	}

	return true;
}

BasicProps::BasicProps() : totalGlobalMem(0), ccMajor(0), ccMinor(0), multiProcessorCount(0), clockRate(0), ECCEnabled(0) {
	memset( uuid, 0, sizeof(uuid) );
	memset( pciId, 0, sizeof(pciId) );
}

// ----------------------------------------------------------------------------

#if defined(WIN32)

cudaError_t
CUDACALL cu_cudaRuntimeGetVersion( int * pver ) {
	if(! pver) { return cudaErrorInvalidValue; }
	* pver = 0;
	cudaError_t ret = cudaErrorNoDevice;

	// On windows, the CUDA runtime isn't installed in the system directory,
	// but we can locate it by looking in the registry.
	// TODO: handle multiple installed runtimes...
	const char * reg_key = "SOFTWARE\\NVIDIA Corporation\\GPU Computing Toolkit\\CUDA";
	const char * reg_val_name = "FirstVersionInstalled";
	HKEY hkey = NULL;
	int bitness = KEY_WOW64_64KEY; //KEY_WOW64_32KEY; KEY_WOW64_64KEY
	int res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_key, 0, KEY_READ | bitness, &hkey);
	if (res != ERROR_SUCCESS) {
		return ret;
	}
	char version[100];
	memset(version, 0, sizeof(version));
	DWORD vtype = REG_SZ, cbData = sizeof(version);
	res = RegQueryValueEx(hkey, reg_val_name, NULL, &vtype, (unsigned char*)version, &cbData);
	if (res == ERROR_SUCCESS) {
		// the value of this key is something like "v1.1", we want to strip off the v and
		// convert the 1.1 into two integers. so we can pack them back together as 0x1010
		version[sizeof(version)-1] = 0;
		const char * pv = version;
		while (*pv == ' ' || *pv == 'v' || *pv == 'V') ++pv;
		int whole=0, fract=0;
		if (2 == sscanf(pv, "%d.%d", &whole, &fract)) {
			*pver = (whole * 1000) + ((fract*10) % 100);
			ret = cudaSuccess;
		}
	}
	RegCloseKey(hkey);

	return ret;
}

#endif /* defined(WIN32) */

// ----------------------------------------------------------------------------

cudaError_t basicPropsFromCudaProps(cudaDevicePropStrings * dps, cudaDevicePropInts * dpi, BasicProps * p)
{
	p->name = dps->name;
	p->totalGlobalMem = dpi->totalGlobalMem;
	p->ccMajor = dpi->ccMajor;
	p->ccMinor = dpi->ccMinor;
	p->clockRate = dpi->clockRate;
	p->multiProcessorCount = dpi->multiProcessorCount;
	p->ECCEnabled = dpi->ECCEnabled;
	memcpy(p->uuid, dps->uuid, sizeof(p->uuid));
	if (dpi->pciBusID || dpi->pciDeviceID) {
		sprintf(p->pciId, "%04x:%02x:%02x.0", dpi->pciDomainID, dpi->pciBusID, dpi->pciDeviceID);
	}
	return cudaSuccess;
}

cudaError_t CUDACALL cuda9_getBasicProps(int devID, BasicProps * p) {
	cudaDevicePropBuffer buf;
	memset(buf.props, 0, sizeof(buf.props));
	cudaError_t err = cudaGetDevicePropertiesOfIndeterminateStructure(&buf, devID);
	if (cudaSuccess != err)
		return err;

	const int ints_offset = 256;
	cudaDevicePropStrings * dps = (cudaDevicePropStrings *)(&buf);
	cudaDevicePropInts * dpi = (cudaDevicePropInts *)(&buf.props[ints_offset]);

	return basicPropsFromCudaProps(dps, dpi, p);
}

cudaError_t CUDACALL cuda10_getBasicProps(int devID, BasicProps * p) {
	cudaDevicePropBuffer buf;
	memset(buf.props, 0, sizeof(buf.props));
	cudaError_t err = cudaGetDevicePropertiesOfIndeterminateStructure(&buf, devID);
	if (cudaSuccess != err)
		return err;

	// skip over the integers, and align on a size_t boundary
	const int ints_offset = (sizeof(cudaDevicePropStrings) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);
	cudaDevicePropStrings * dps = (cudaDevicePropStrings *)(&buf);
	cudaDevicePropInts * dpi = (cudaDevicePropInts *)(&buf.props[ints_offset]);
	memcpy(p->uuid, dps->uuid, sizeof(dps->uuid));
	return basicPropsFromCudaProps(dps, dpi, p);
}

cudaError_t CUDACALL cu_getBasicProps(int devID, BasicProps * p) {
	cudev dev;
	cudaError_t res = cuDeviceGet(&dev, devID);
	if (cudaSuccess == res) {
		char name[256];
		memset(name, 0, sizeof(name));
		cuDeviceGetName(name, sizeof(name), dev);
		p->name = name;
		if (cuDeviceGetUuid) cuDeviceGetUuid(p->uuid, dev);
		if (cuDeviceGetPCIBusId) cuDeviceGetPCIBusId(p->pciId, sizeof(p->pciId), dev);
		cuDeviceComputeCapability(&p->ccMajor, &p->ccMinor, dev);
		cuDeviceGetAttribute(&p->clockRate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, dev);
		cuDeviceGetAttribute(&p->multiProcessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
		cuDeviceGetAttribute(&p->ECCEnabled, CU_DEVICE_ATTRIBUTE_ECC_ENABLED, dev);
	}
	return res;
}
