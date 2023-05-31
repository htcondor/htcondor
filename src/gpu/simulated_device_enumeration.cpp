#include <time.h>
#include <string.h>

#include <string>
#include <vector>
#include <set>

// For pi_dynlink.h
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>
#endif

#include "pi_dynlink.h"

#include "nvml_stub.h"
#include "cuda_header_doc.h"
#include "cuda_device_enumeration.h"

//From Dumbo.
//DetectedGPUs="GPU-c4a646d7, GPU-6a96bd13"
//CUDACoresPerCU=64
//CUDADriverVersion=11.20
//CUDAMaxSupportedVersion=11020
//GPU_6a96bd13Capability=7.5
//GPU_6a96bd13ClockMhz=1770.00
//GPU_6a96bd13ComputeUnits=72
//GPU_6a96bd13DeviceName="TITAN RTX"
//GPU_6a96bd13DevicePciBusId="0000:AF:00.0"
//GPU_6a96bd13DeviceUuid="6a96bd13-70bc-6494-6d62-1b77a9a7f29f"
//GPU_6a96bd13ECCEnabled=false
//GPU_6a96bd13GlobalMemoryMb=24220
//GPU_c4a646d7Capability=7.0
//GPU_c4a646d7ClockMhz=1380.00
//GPU_c4a646d7ComputeUnits=80
//GPU_c4a646d7DeviceName="Tesla V100-PCIE-16GB"
//GPU_c4a646d7DevicePciBusId="0000:3B:00.0"
//GPU_c4a646d7DeviceUuid="c4a646d7-aa14-1dd1-f1b0-57288cda864d"
//GPU_c4a646d7ECCEnabled=true
//GPU_c4a646d7GlobalMemoryMb=16160

//From Igor, MIG capable GPU and 7 MIG devices enumerated by 9.1.0
//DetectedGPUs=GPU-cb91fc08, GPU-MIG-3167, GPU-MIG-5bc6, GPU-MIG-5daa, GPU-MIG-9654, GPU-MIG-987e, GPU-MIG-c1df, GPU-MIG-e84b
//CUDADriverVersion=11.40
//CUDAMaxSupportedVersion=11040
//GPU_MIG_3167DeviceUuid="MIG-3167cf3e-b048-5a67-8134-a2610532c94e"
//GPU_MIG_3167GlobalMemoryMb=4861
//GPU_MIG_5bc6DeviceUuid="MIG-5bc689c8-e6d8-5f8c-b51e-35a01df05f5f"
//GPU_MIG_5bc6GlobalMemoryMb=4861
//GPU_MIG_5daaDeviceUuid="MIG-5daab7d7-3e0a-5b76-bf22-8bc5fc94a3e6"
//GPU_MIG_5daaGlobalMemoryMb=4861
//GPU_MIG_9654DeviceUuid="MIG-9654dd54-5c9f-54be-adf0-55269caad1f2"
//GPU_MIG_9654GlobalMemoryMb=4861
//GPU_MIG_987eDeviceUuid="MIG-987e0eb2-08b2-55ca-9663-89eaaf68156a"
//GPU_MIG_987eGlobalMemoryMb=3512
//GPU_MIG_c1dfDeviceUuid="MIG-c1df38f3-a7ad-5465-8941-f0260e698979"
//GPU_MIG_c1dfGlobalMemoryMb=4861
//GPU_MIG_e84bDeviceUuid="MIG-e84b0b57-e50c-50df-a261-bc0a05671e0a"
//GPU_MIG_e84bGlobalMemoryMb=4861
//GPU_cb91fc08Capability=8.0
//GPU_cb91fc08DeviceName="NVIDIA A100-SXM4-40GB MIG 1g.5gb"
//GPU_cb91fc08DevicePciBusId="0000:00:04.0"
//GPU_cb91fc08DeviceUuid="cb91fc08-a9a0-9087-ce0c-5d09d5f00f03"
//GPU_cb91fc08ECCEnabled=true
//GPU_cb91fc08GlobalMemoryMb=4864

// From gpulab2004 4 A100's
//CUDADriverVersion = 11.2
//CUDAMaxSupportedVersion = 11020
//GPU_124d06a7DevicePciBusId = "0000:01:00.0"
//GPU_124d06a7DeviceUuid = "124d06a7-6642-3962-9afa-c86c31b9a7e6"
//GPU_20e62ffcDevicePciBusId = "0000:C1:00.0"
//GPU_20e62ffcDeviceUuid = "20e62ffc-155e-49ad-04e1-ffacfc109ce3"
//GPU_8f9c2d75DevicePciBusId = "0000:81:00.0"
//GPU_8f9c2d75DeviceUuid = "8f9c2d75-1b7d-5026-b177-283d13ddbb90"
//GPU_c6cfdc9cDevicePciBusId = "0000:41:00.0"
//GPU_c6cfdc9cDeviceUuid = "c6cfdc9c-63df-c5b3-3605-7bc4b2545434"
//CUDACapability = 8.0
//CUDADeviceName = "A100-SXM4-40GB"
//CUDAECCEnabled = true
//CUDAGlobalMemoryMb = 40536

// 8 A100's from AWS (nested), one in MIG mode
//DetectedGPUs="GPU-8aa54555, MIG-0caf1ce3-fcd4-59da-8564-764000e3ace9, MIG-207f657b-6f50-5c1a-94f0-01d07ff4cdb0, GPU-775e454e, GPU-d79a3765, GPU-a6e658d5, GPU-c73e918e, GPU-e627fc34, GPU-5368a050, GPU-5d2744da"
//Common=[ DriverVersion=11.40; MaxSupportedVersion=11040; ]
//GPU_5368a050=[ id="GPU-5368a050"; Capability=8.0; ClockMhz=1410.00; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB"; DevicePciBusId="0000:A0:1C.0"; DeviceUuid="GPU-5368a050-ed79-fab9-5cba-0d37c5f12c81"; DieTempC=28; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=40536; PowerUsage_mw=52603; ]
//GPU_5d2744da=[ id="GPU-5d2744da"; Capability=8.0; ClockMhz=1410.00; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB"; DevicePciBusId="0000:A0:1D.0"; DeviceUuid="GPU-5d2744da-c329-024c-6477-17d879625691"; DieTempC=27; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=40536; PowerUsage_mw=49850; ]
//GPU_775e454e=[ id="GPU-775e454e"; Capability=8.0; ClockMhz=1410.00; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB"; DevicePciBusId="0000:10:1D.0"; DeviceUuid="GPU-775e454e-c504-a1bb-22c6-3db1b940058d"; DieTempC=25; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=40536; PowerUsage_mw=49049; ]
//GPU_a6e658d5=[ id="GPU-a6e658d5"; Capability=8.0; ClockMhz=1410.00; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB"; DevicePciBusId="0000:20:1D.0"; DeviceUuid="GPU-a6e658d5-41c2-c7e4-6f66-912a14127670"; DieTempC=27; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=40536; PowerUsage_mw=50891; ]
//GPU_c73e918e=[ id="GPU-c73e918e"; Capability=8.0; ClockMhz=1410.00; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB"; DevicePciBusId="0000:90:1C.0"; DeviceUuid="GPU-c73e918e-cc34-67c5-bc49-1aa14ccaa76e"; DieTempC=27; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=40536; PowerUsage_mw=54073; ]
//GPU_d79a3765=[ id="GPU-d79a3765"; Capability=8.0; ClockMhz=1410.00; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB"; DevicePciBusId="0000:20:1C.0"; DeviceUuid="GPU-d79a3765-a737-9880-9f25-c901ef668a0b"; DieTempC=27; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=40536; PowerUsage_mw=53276; ]
//GPU_e627fc34=[ id="GPU-e627fc34"; Capability=8.0; ClockMhz=1410.00; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB"; DevicePciBusId="0000:90:1D.0"; DeviceUuid="GPU-e627fc34-bc78-bc31-d502-b26fad63dea8"; DieTempC=26; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=40536; PowerUsage_mw=52970; ]
//GPU_8aa54555=[ id="GPU-8aa54555"; Capability=8.0; ClockMhz=1410.00; ComputeUnits=42; CoresPerCU=64; DeviceName="NVIDIA A100-SXM4-40GB MIG 3g.20gb"; DevicePciBusId="0000:10:1C.0"; DeviceUuid="8aa54555-90e8-79b7-e5f8-e4b2b3e8fcfe"; DieTempC=25; ECCEnabled=true; EccErrorsDoubleBit=0; EccErrorsSingleBit=0; GlobalMemoryMb=20096; PowerUsage_mw=40055; ]
//MIG_0caf1ce3_fcd4_59da_8564_764000e3ace9=[ id="MIG-0caf1ce3-fcd4-59da-8564-764000e3ace9"; ComputeUnits=42; DeviceUuid="MIG-0caf1ce3-fcd4-59da-8564-764000e3ace9"; GlobalMemoryMb=20086; ]
//MIG_207f657b_6f50_5c1a_94f0_01d07ff4cdb0=[ id="MIG-207f657b-6f50-5c1a-94f0-01d07ff4cdb0"; ComputeUnits=42; DeviceUuid="MIG-207f657b-6f50-5c1a-94f0-01d07ff4cdb0"; GlobalMemoryMb=20082; ]


//
// tables of simulated cuda devices, so we can test HTCondor startd and starter without 
// actually having a GPU.
//

struct _simulated_cuda_device {
	const char * name;
	int SM;    // capability 0xMm (hexidecimal notation), M = SM Major version, and m = SM minor version
	int clockRate;
	int multiProcessorCount;
	int ECCEnabled;
	size_t totalGlobalMem;
};

struct _simulated_mig_devices {
	unsigned int migCount;
	unsigned int migMode;   // MIG configuration, here are a limited number of partitioning schemes
	struct {
		int memory;
	} inst[7];
};

struct _simulated_cuda_config {
	int driverVer;
	int deviceCount;
	const struct _simulated_cuda_device * device;
	const struct _simulated_mig_devices * mig;
};

static const struct _simulated_cuda_device GeForceGT330 = { "GeForce GT 330",  0x12, 1340*1000, 12,  0, 1024*1024*1024 };
static const struct _simulated_cuda_device GeForceGTX480 = { "GeForce GTX 480", 0x20, 1400*1000, 15, 0, 1536*1024*1024 };
static const struct _simulated_cuda_device TeslaV100 = { "Tesla V100-PCIE-16GB",  0x70, 1380*1000, 80,  1, (size_t)24220*1024*1024 };
static const struct _simulated_cuda_device TitanRTX = { "TITAN RTX",  0x75, 1770*1000, 72,  0, (size_t)24220*1024*1024 };
static const struct _simulated_cuda_device A100 = { "A100-SXM4-40GB", 0x80, 1410*1000, 42, 1, (size_t)40536*1024*1204 };
static const struct _simulated_cuda_device A100Mig1g = { "NVIDIA A100-SXM4-40GB MIG 1g.5gb", 0x80, 1410*1000, 42, 1, (size_t)4864*1024*1204 };
static const struct _simulated_cuda_device A100Mig3g = { "NVIDIA A100-SXM4-40GB MIG 3g.20gb", 0x80, 1410*1000, 42, 1, (size_t)20096*1024*1204 };
static const struct _simulated_mig_devices Mig3g20 = { 2, 3, {{20086}, {20082}, {0}, {0}, {0}, {0}, {0}} };
static const struct _simulated_mig_devices Mig1g5 = { 7, 1, {{4861}, {4861}, {4861}, {4861}, {4861}, {4861}, {4861}} };
static const struct _simulated_cuda_config aSimConfig[] = {
	{6000, 1, &GeForceGT330, NULL },   // 0
	{4020, 2, &GeForceGTX480, NULL },  // 1
	// from dumbo
	{11020, 1, &TeslaV100, NULL },     // 2
	{11020, 1, &TitanRTX, NULL },      // 3
	// mig capable devices
	{11020, 1, &A100, NULL },          // 4 (not in mig mode)
	{11020, 1, &A100Mig3g, &Mig3g20 },     // 5 (interpolated)
	{11040, 1, &A100Mig1g, &Mig1g5 },     // 6 (from Igor)
	{11040, 1, &A100Mig3g, &Mig3g20 },     // 7 (from AWS)
};

const int sim_index_max = (int)(sizeof(aSimConfig)/sizeof(aSimConfig[0]));
bool sim_enable_mig = false;

static std::vector<const struct _simulated_cuda_config *> sim_devices;
bool setupSimulatedDevices(const char * args)
{
	if ( ! args) {
		// if no args passed, use the first device as the default
		sim_devices.push_back(&aSimConfig[0]);
		return true;
	}

	// if the first arg is equal to the size of the device table, simulate one of each device
	if (atoi(args) == sim_index_max) {
		sim_enable_mig = true;
		// one of each device
		for (int dev = 0; dev < sim_index_max; ++dev)
			sim_devices.push_back(&aSimConfig[dev]);
		return true;
	}

	// args is a list of sim_index,count pairs
	while (args && args[0]) {
		char * endp = nullptr;
		int ix = strtol(args, &endp, 10);
		if (ix < 0 || ix > sim_index_max || endp==args) {
			return false;
		}
		int count = 1;
		if (endp && *endp == ',') {
			args = endp + 1;
			count = strtol(args, &endp, 10);
		}
		if (endp && *endp == ',') ++endp;
		args = endp;
		for (int dev = 0; dev < count; ++dev) {
			sim_devices.push_back(&aSimConfig[ix]);
		}
		if (ix > 1) sim_enable_mig = true;
	}

	return true;
}

cudaError_t CUDACALL sim_cudaGetDeviceCount(int* pdevs) {
	*pdevs = (int)sim_devices.size();
	return cudaSuccess;
}

cudaError_t CUDACALL sim_cudaDriverGetVersion(int* pver) {
	if (sim_devices.empty())
		return cudaErrorInvalidValue;
	*pver = sim_devices[0]->driverVer;
	return cudaSuccess; 
}

static unsigned char* sim_makeuuid(int devID, unsigned int migID=0x34) {
	static unsigned char uuid[16] = {0xa1,0x22,0x33,0x34,  0x44,0x45, 0x56,0x67, 0x89,0x9a, 0xab,0xbc,0xcd,0xde,0xef,0xf0 };
	uuid[0] = (unsigned char)(((devID*0x11) + 0xa0) & 0xFF);
	uuid[3] = (unsigned char)(migID & 0xFF);
	return uuid;
}

cudaError_t CUDACALL sim_getBasicProps(int devID, BasicProps * p) {
	if (sim_devices.empty())
		return cudaErrorNoDevice;
	if (devID < 0 || devID >= (int)sim_devices.size())
		return cudaErrorInvalidDevice;
	const struct _simulated_cuda_device * dev = sim_devices[devID]->device;
	p->name = dev->name;
	p->setUUIDFromBuffer( sim_makeuuid(devID) );
	snprintf(p->pciId, sizeof(p->pciId), "0000:%02x:00.0", devID + 0x40);
	p->ccMajor = (dev->SM & 0xF0) >> 4;
	p->ccMinor = (dev->SM & 0x0F);
	p->multiProcessorCount = dev->multiProcessorCount;
	p->clockRate = dev->clockRate;
	p->totalGlobalMem = dev->totalGlobalMem;
	p->ECCEnabled = dev->ECCEnabled;
	return cudaSuccess;
}

int sim_jitter = 0;
nvmlReturn_t sim_nvmlInit(void) { sim_jitter = (int)(time(NULL) % 10); return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetFanSpeed(nvmlDevice_t dev, unsigned int * pval) { *pval = 9+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetPowerUsage(nvmlDevice_t dev, unsigned int * pval) { *pval = 29+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetTemperature(nvmlDevice_t dev, nvmlTemperatureSensors_t /*sensor*/, unsigned int * pval) { *pval = 89+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS;}
nvmlReturn_t sim_nvmlDeviceGetTotalEccErrors(nvmlDevice_t /*dev*/, nvmlMemoryErrorType_t /*met*/, nvmlEccCounterType_t /*mec*/, unsigned long long * pval) { *pval = 0; /*sim_jitter-1+(int)dev;*/ return NVML_SUCCESS; }

inline nvmlDevice_t sim_index_to_nvmldev(unsigned int simindex, unsigned int migindex=0x34) {
	size_t mask = 0x100 + (migindex * (size_t)0x1000);
	return (nvmlDevice_t)(((size_t)(simindex)) | mask);
}
inline unsigned int nvmldev_to_sim_index(nvmlDevice_t device) {
	return (unsigned int)((size_t)(device) & 0xFF);
}
inline unsigned int nvmldev_to_mig_index(nvmlDevice_t device) {
	return (unsigned int)((((size_t)(device)) / 0x1000) & 0xFF);
}
inline unsigned int nvmldev_is_mig(nvmlDevice_t device) {
	return nvmldev_to_mig_index(device) < 8;
}

nvmlReturn_t
sim_nvmlDeviceGetHandleByIndex(unsigned int devID, nvmlDevice_t * pdev) {
	if (devID > (unsigned int)sim_devices.size())
		return NVML_ERROR_NOT_FOUND;
	* pdev = sim_index_to_nvmldev(devID);
	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetHandleByUUID(const char * uuid, nvmlDevice_t * pdev) {
	unsigned int devID=0, migID=0;

	if( sscanf(uuid, "%02x2233%02x-4445-5667-899aabbccddeeff0", & devID, &migID) != 2 ) {
		return NVML_ERROR_NOT_FOUND;
	}
	devID = (devID & 0x0F);
	if (migID == 0x34) { // is this a MIG uuid?
		migID = 0;
	} else if (migID > 7) {
		return NVML_ERROR_NOT_FOUND;
	}

	* pdev = sim_index_to_nvmldev(devID, migID);
	return NVML_SUCCESS;
};

nvmlReturn_t
sim_findNVMLDeviceHandle(const std::string & uuid, nvmlDevice_t * device) {
	return nvmlDeviceGetHandleByUUID( uuid.c_str(), device );
}

nvmlReturn_t
sim_nvmlDeviceGetCount (unsigned int * count) {
	int num = 0;
	if (sim_cudaGetDeviceCount(&num) == cudaSuccess) {
		*count = (unsigned int)num;
		return NVML_SUCCESS;
	}
	return NVML_ERROR_NOT_SUPPORTED;
}

// given an nvml device handle, return the config entry for that device
static nvmlReturn_t sim_getconfig(nvmlDevice_t device, const struct _simulated_cuda_config *& config) {
	config = nullptr;

	int devID = nvmldev_to_sim_index(device);
	if (devID < 0 || devID >= (int)sim_devices.size())
		return NVML_ERROR_NOT_FOUND;
	config = sim_devices[devID];
	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetMaxMigDeviceCount(nvmlDevice_t device, unsigned int * count ) {
	const struct _simulated_cuda_config * config = nullptr;
	nvmlReturn_t ret = sim_getconfig(device, config);
	if (ret != NVML_SUCCESS) { return ret; }
	if ( ! config->mig) { 
		if (config->driverVer < 10000) {
			return NVML_ERROR_NOT_SUPPORTED;
		} else {
			*count = 0;
			return NVML_SUCCESS;
		}
	}
	* count = config->mig->migCount;
	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetMigDeviceHandleByIndex(nvmlDevice_t device, unsigned int index, nvmlDevice_t* migDevice ) {
	const struct _simulated_cuda_config * config = nullptr;
	nvmlReturn_t ret = sim_getconfig(device, config);
	if (ret != NVML_SUCCESS) { return ret; }

	if ( ! config->mig || nvmldev_is_mig(device)) { return NVML_ERROR_NOT_SUPPORTED; }
	if (index >= config->mig->migCount) { return NVML_ERROR_NOT_FOUND; }
	int devID = nvmldev_to_sim_index(device);
	* migDevice = sim_index_to_nvmldev(devID, index);
	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetUUID(nvmlDevice_t device, char *buf, unsigned int bufsize ) {
	const struct _simulated_cuda_config * config = nullptr;
	nvmlReturn_t ret = sim_getconfig(device, config);
	if (ret != NVML_SUCCESS) { return ret; }
	int devID = nvmldev_to_sim_index(device);
	std::string bogus_uuid;
	if (nvmldev_is_mig(device)) {
		unsigned int migID = nvmldev_to_mig_index(device);
		// first turn the uuid into a string.
		char uuidbuf[NVML_DEVICE_UUID_V2_BUFFER_SIZE];
		// then turn that into a nvml "uuid" which is really a string containing a uuid and prefix/suffix
		if (config->driverVer >= 11040) {
			bogus_uuid = "MIG-";
			print_uuid(uuidbuf, NVML_DEVICE_UUID_V2_BUFFER_SIZE, sim_makeuuid(devID, migID));
			bogus_uuid += uuidbuf;
		} else {
			bogus_uuid = "MIG-GPU-";
			print_uuid(uuidbuf, NVML_DEVICE_UUID_V2_BUFFER_SIZE, sim_makeuuid(devID));
			bogus_uuid += uuidbuf;
			snprintf(uuidbuf, sizeof(uuidbuf), "/%d/0", migID);
			bogus_uuid += uuidbuf;
		}
	} else {
		// first turn the uuid into a string.
		char uuidbuf[NVML_DEVICE_UUID_V2_BUFFER_SIZE];
		print_uuid(uuidbuf, NVML_DEVICE_UUID_V2_BUFFER_SIZE, sim_makeuuid(devID));
		bogus_uuid = "GPU-";
		bogus_uuid += uuidbuf;
	}

	strncpy(buf, bogus_uuid.c_str(), bufsize-1);
	buf[bufsize - 1] = 0;

	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetName(nvmlDevice_t device, char *buf, unsigned int /* bufsize */) {
	const struct _simulated_cuda_config * config = nullptr;
	nvmlReturn_t ret = sim_getconfig(device, config);
	if (ret != NVML_SUCCESS) { return ret; }
	if (nvmldev_is_mig(device)) {
		// TODO: is driver 470 different than driver 450 ?
		// strcpy(buf, config->device->name);
		return NVML_ERROR_NOT_FOUND;
	} else {
		strcpy(buf, config->device->name);
	}
	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t * memory ) {
	const struct _simulated_cuda_config * config = nullptr;
	nvmlReturn_t ret = sim_getconfig(device, config);
	if (ret != NVML_SUCCESS) { return ret; }
	if (nvmldev_is_mig(device)) {
		memory->total = config->mig->inst[nvmldev_to_mig_index(device)].memory * (size_t)(1024*1024);
	} else {
		memory->total = config->device->totalGlobalMem;
	}
	memory->free = memory->total;
	memory->used = 0;
	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetEccMode( nvmlDevice_t device, nvmlEnableState_t* current, nvmlEnableState_t* /* pending */ ) {
	const struct _simulated_cuda_config * config = nullptr;
	nvmlReturn_t ret = sim_getconfig(device, config);
	if (ret != NVML_SUCCESS) { return ret; }
	int enabled = 0;
	if (nvmldev_is_mig(device)) { // TODO: per MIG ecc?
		enabled = config->device->ECCEnabled;
	} else {
		enabled = config->device->ECCEnabled;
	}
	* current = enabled ? NVML_FEATURE_ENABLED : NVML_FEATURE_DISABLED;
	return NVML_SUCCESS;
}

nvmlReturn_t
sim_nvmlDeviceGetMaxClockInfo( nvmlDevice_t device, nvmlClockType_t /*ct*/, unsigned int * clock ) {
	const struct _simulated_cuda_config * config = nullptr;
	nvmlReturn_t ret = sim_getconfig(device, config);
	if (ret != NVML_SUCCESS) { return ret; }
	int clockRate = 0;
	if (nvmldev_is_mig(device)) { // TODO: per MIG ecc?
		clockRate = config->device->clockRate;
	} else {
		clockRate = config->device->clockRate;
	}
	// table in Khz, but NVML wants MHz
	* clock = clockRate / 1000;
	return NVML_SUCCESS;
}

const char * 
sim_nvmlErrorString( nvmlReturn_t rt)
{
	switch (rt) {
	case NVML_SUCCESS: return "The operation was successful";
	case NVML_ERROR_UNINITIALIZED: return "NVML was not first initialized with nvmlInit()";
	case NVML_ERROR_INVALID_ARGUMENT: return "A supplied argument is invalid";
	case NVML_ERROR_NOT_SUPPORTED: return "The requested operation is not available on target device";
	case NVML_ERROR_NO_PERMISSION: return "The current user does not have permission for operation";
	case NVML_ERROR_ALREADY_INITIALIZED: return "Deprecated: Multiple initializations are now allowed through ref counting";
	case NVML_ERROR_NOT_FOUND: return "A query to find an object was unsuccessful";
	case NVML_ERROR_INSUFFICIENT_SIZE: return "An input argument is not large enough";
	case NVML_ERROR_INSUFFICIENT_POWER: return "A device's external power cables are not properly attached";
	case NVML_ERROR_DRIVER_NOT_LOADED: return "NVIDIA driver is not loaded";
	case NVML_ERROR_TIMEOUT: return "User provided timeout passed";
	case NVML_ERROR_IRQ_ISSUE: return "NVIDIA Kernel detected an interrupt issue with a GPU";
	case NVML_ERROR_LIBRARY_NOT_FOUND: return "NVML Shared Library couldn't be found or loaded";
	case NVML_ERROR_FUNCTION_NOT_FOUND: return "Local version of NVML doesn't implement this function";
	case NVML_ERROR_CORRUPTED_INFOROM: return "infoROM is corrupted";
	default: return "unknown";
	}
}

void
setSimulatedCUDAFunctionPointers() {
    getBasicProps = sim_getBasicProps;
    cuDeviceGetCount = sim_cudaGetDeviceCount;
    cudaDriverGetVersion = sim_cudaDriverGetVersion;
}

bool
setSimulatedNVMLFunctionPointers() {
    nvmlInit = sim_nvmlInit;
    nvmlShutdown = sim_nvmlInit;

    nvmlDeviceGetFanSpeed = sim_nvmlDeviceGetFanSpeed;
    nvmlDeviceGetPowerUsage = sim_nvmlDeviceGetPowerUsage;
    nvmlDeviceGetTemperature = sim_nvmlDeviceGetTemperature;
    nvmlDeviceGetTotalEccErrors = sim_nvmlDeviceGetTotalEccErrors;
    nvmlDeviceGetHandleByUUID = sim_nvmlDeviceGetHandleByUUID;
    nvmlDeviceGetHandleByIndex = sim_nvmlDeviceGetHandleByIndex;
	nvmlDeviceGetUUID = sim_nvmlDeviceGetUUID;
	nvmlErrorString = sim_nvmlErrorString;

	findNVMLDeviceHandle = sim_findNVMLDeviceHandle;
	if (sim_enable_mig) { // for later versions of the runtime, enable MIG
		nvmlDeviceGetCount = sim_nvmlDeviceGetCount;
		nvmlDeviceGetName = sim_nvmlDeviceGetName;
		nvmlDeviceGetMemoryInfo = sim_nvmlDeviceGetMemoryInfo;
		nvmlDeviceGetEccMode = sim_nvmlDeviceGetEccMode;
		nvmlDeviceGetMaxClockInfo = sim_nvmlDeviceGetMaxClockInfo;
		nvmlDeviceGetMaxMigDeviceCount = sim_nvmlDeviceGetMaxMigDeviceCount;
		nvmlDeviceGetMigDeviceHandleByIndex = sim_nvmlDeviceGetMigDeviceHandleByIndex;
	}
	return sim_enable_mig;
}
