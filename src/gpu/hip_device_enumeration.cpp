#include <stdlib.h>
#include <string.h>

#include <vector>
#include <string>

// For pi_dynlink.h
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>
#endif

#include "pi_dynlink.h"
#include "hip_header_doc.h"
#include "hip_device_enumeration.h"

#include "print_error.h"

static struct {
	hipDeviceCount_t hipGetDeviceCount{nullptr};
	hipDeviceProperties_t hipGetDeviceProperties{nullptr};
	hipDriverGetVersion_t hipDriverGetVersion{nullptr};
	hipDeviceGetUuid_t hipDeviceGetUuid{nullptr};
} hip;

extern void hex_dump(FILE* out, const unsigned char * buf, size_t cb, int offset);

static int hip_device_count = 0;
static int hip_was_initialized = 0;
static std::vector<hipDeviceProp_t> hip_platforms;

int hip_Init(void) {

	print_error(MODE_DIAGNOSTIC_MSG, "diag: hip_Init()\n");
	if (hip_was_initialized) {
		return 0;
	}
	hip_was_initialized = 1;

	if (! hip.hipGetDeviceCount) {
		return -1;
	}

	// We use an allocation rather than a stack object here to avoid trashing the stack
	// if the API changes, and allocate extra space also to avoid crashing.
	size_t cb = sizeof(hipDeviceProp_t)*2;
	hipDeviceProp_t * props = (hipDeviceProp_t*)malloc(cb);
	std::string name;

	int hPlatforms = 0;
	hipError_t hip_return = hip.hipGetDeviceCount(&hPlatforms);
	print_error(MODE_DIAGNOSTIC_MSG, "diag: hipDevice count: %d \n",hPlatforms);	
	if (hip_return == hipSuccess && hPlatforms > 0) {
		hip_platforms.reserve(hPlatforms);
		for (int ii = 0; ii < hPlatforms; ++ii) {
			if ( ! hip.hipGetDeviceProperties) {
				print_error(MODE_DIAGNOSTIC_MSG, "diag: hipGetDevicePropertiesR0600 function ptr is NULL!\n");
				continue;
			}

			memset(props, 0, cb);
			hipError_t err = hip.hipGetDeviceProperties(props,ii);
			name.assign(props->name, 64); // grab the first 64 chars for logging.
			print_error(MODE_DIAGNOSTIC_MSG,
				"diag: prop query [%d] result:%d -> %s %02x%02x%02x%02x...\n",
				ii, err, name.c_str(),
				props->uuid.bytes[0], props->uuid.bytes[1], props->uuid.bytes[2], props->uuid.bytes[3]);
			if (hip.hipDeviceGetUuid) {
				hipUUID_t & uuid = props->uuid;
				err = hip.hipDeviceGetUuid(&props->uuid, ii);
				print_error(MODE_DIAGNOSTIC_MSG,
					"diag: uuid query [%d] result:%d -> %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n", ii, err,
					uuid.bytes[0], uuid.bytes[1], uuid.bytes[2], uuid.bytes[3],
					uuid.bytes[4], uuid.bytes[5],  uuid.bytes[6], uuid.bytes[7],  uuid.bytes[8], uuid.bytes[9],
					uuid.bytes[10], uuid.bytes[11], uuid.bytes[12], uuid.bytes[13], uuid.bytes[14], uuid.bytes[15]
					);
			}
			if (g_diagnostic && g_verbose) {
				// mark the point where a large sequence of int and size_t fields begin
				int ints_offset = (int)(size_t)((const char*)props - (const char *)(&props->totalGlobalMem));
				hex_dump(stdout, (const unsigned char*)props, sizeof(hipDeviceProp_t), ints_offset);
			}

			hip_platforms.push_back(*props);
		}
	}

	// enable logging in oclGetInfo if verbose

	hip_device_count = (int)hPlatforms;
	return 0;
}

int HIP_CALL hip_GetDeviceCount(int * pcount) {
	hip_Init();
	*pcount = hip_device_count;
	return HIP_SUCCESS;
}

dlopen_return_t
loadHipLibrary() {
#if       defined(WIN32)
	const char * hip_library = "amdhip64.dll";
#else  /* defined(WIN32) */
	const char * hip_library = "libamdhip64.so.6";
#endif /* defined(WIN32) */

	dlopen_return_t hip_handle = dlopen( hip_library, RTLD_LAZY );
	if(! hip_handle) {
		print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", hip_library, dlerror());
	} else {
		print_error(MODE_DIAGNOSTIC_MSG, "Loaded HIP library '%s'\n", hip_library);
	}

	dlerror(); // reset error
	return hip_handle;
}


hipError_t HIP_API_CALL sim_hipDeviceCount(int * count) { *count = hip_device_count; return hipSuccess; }
hipError_t HIP_API_CALL sim_hipDriverVersion(int* ver) { *ver = 60342131;  return hipSuccess; }
hipError_t HIP_API_CALL sim_hipDeviceProperties(hipDeviceProp_t * props, int dev) {
	if (dev < 0 || dev >= hip_device_count) {
		return hipErrorInvalidValue;
	}
	strcat(props->name, "AMD Instinct MI300A");
	memset(props->uuid.bytes, '1'+dev, sizeof(props->uuid.bytes));
	props->totalGlobalMem = 131072 * ((size_t)1024.*1024);
	props->major = 9;
	props->minor = 4;
	props->clockRate = 2100 * 1000;
	props->multiProcessorCount = 228;
	props->ECCEnabled = true;
	props->warpSize = 64;
	props->unifiedAddressing = 1;
	props->pciDomainID = dev;
	props->pciBusID = 2;
	props->pciDeviceID = 0;

	return hipSuccess;
}

// so we can add hipDetection via ROCM 6 to the gpu properties
#define HIP_LIB_MAJOR_VERSION 6

dlopen_return_t
setHIPFunctionPointers(int simulate) {
	if (simulate) {
		hip.hipGetDeviceCount = sim_hipDeviceCount;
		hip.hipDriverGetVersion = sim_hipDriverVersion;
		hip.hipGetDeviceProperties = sim_hipDeviceProperties;
		hip_device_count = simulate;
		return (dlopen_return_t)1;
	}

	dlopen_return_t hip_handle = loadHipLibrary();
	if(! hip_handle) { return NULL; }

	hip.hipGetDeviceCount = (hipDeviceCount_t) dlsym(hip_handle, "hipGetDeviceCount");
	hip.hipGetDeviceProperties = (hipDeviceProperties_t) dlsym(hip_handle, "hipGetDevicePropertiesR0600");
	hip.hipDriverGetVersion = (hipDriverGetVersion_t) dlsym(hip_handle,"hipDriverGetVersion");
	hip.hipDeviceGetUuid = (hipDeviceGetUuid_t) dlsym(hip_handle,"hipDeviceGetUuid");
	print_error(MODE_DIAGNOSTIC_MSG, "diag: HIP function pointers:\n"
		"\t%p : hipGetDeviceCount\n"
		"\t%p : hipGetDevicePropertiesR0600\n"
		"\t%p : hipDriverGetVersion\n"
		"\t%p : hipDeviceGetUuid\n",
		hip.hipGetDeviceCount, hip.hipGetDeviceProperties, hip.hipDriverGetVersion, hip.hipDeviceGetUuid);

	return hip_handle;
}
hipError_t hip_getBasicProps(int dev, BasicProps *bp, int fix_guid_magic);

bool enumerateHIPDevices(std::vector< BasicProps > & devices, int fix_guid_magic){
	print_error(MODE_DIAGNOSTIC_MSG, "diag: enumerateHIPDevices %d\n", hip_device_count);
	hip_Init();
	print_error(MODE_DIAGNOSTIC_MSG, "diag: %d devices found by hip_Init()\n", hip_device_count);
	for( int dev = 0; dev < hip_device_count; ++dev){
		BasicProps bp;
		if( hipSuccess == hip_getBasicProps(dev, &bp, fix_guid_magic) ){
			devices.push_back(bp);
		}
	}
	return true;

}

bool is_null_uuid(const hipUUID & uuid) { for (auto b: uuid.bytes) { if (b) return false; } return true; }
bool is_max_uuid(const hipUUID & uuid) { for (auto b: uuid.bytes) { if (b != (char)0xFF) return false; } return true; }

// ROCM6 in SDSC (the only AMD devices we have been able to test so far)
// is populating the uuid with 16 hex digits rather than 16 random-ish bytes.
// Until they fix this, we cannot use what is returned as the device uuid.
// What amd-smi seems to be doing is parsing the 16 hex digits to get 8 bytes
// and using that to fill in the uuid so that 0xb0c0cb9338716a18 
// becomes the UUID b0ff74a0-0000-1000-80c0-cb9338716a18
// ff74a0-0000-1000-80 is coming from somewhere other than the "uuid" property value
// TJs speculates that the leading ff and -0000-1000-80 are constants and 74a0 is a GPU model number.
// 1xxx-8x indicates a variant 8 version 1 GUID. All variant 8 versions are described
// in rfc9562 https://datatracker.ietf.org/doc/html/rfc9562.html
// A version 1 GUID *should* be a Gregorian time-based GUID, which is not very plausible for a GPU uuid.
// More examples are needed (or better documentation) to know how to turn the hipUUID into a
// valid GPU UUID, for now we will report that there is no UUID since we don't know how to make
// these numbers work with *_VISIBLE_DEVICES
bool is_plausiable_uuid(const hipUUID & uuid)
{
	for (auto b : uuid.bytes) {
		if ((b >= '0' && b <= '9') || (b >= 'a' && b <= 'f')) continue;
		return true;
	}
	return false;
}

static char known_hex(const char c0, const char c1) {
	unsigned int ch = c0, cl = c1;
	if (ch > '9') ch += 9;
	if (cl > '9') cl += 9;
	return ((ch & 0x0F) << 4) | (cl & 0xF);
}

// convert boad ROCM6 UUID (16 hex digits) into a valid guid by
// using the hex digits to populate the X's XXuuGGGG-uuuu-uuuu-uuXX-XXXXXXXXXXXX
// and fix_guid_magic to populate the G's, with constant values for the u's
// this is the pattern that hip-smi seems to be using in SDSC for their APUs
void fix_rocm6_uuid(hipUUID & out, const hipUUID & bad, int fix_guid_magic) {
	const char * bb = bad.bytes;
	out.bytes[0] = known_hex(bb[0], bb[1]); bb += 2;
	out.bytes[1] = 0xFF;
	out.bytes[2] = (fix_guid_magic & 0xFF00) >> 8;
	out.bytes[3] = (fix_guid_magic & 0x00FF);
	out.bytes[4] = 0;
	out.bytes[5] = 0;
	out.bytes[6] = 0x10;
	out.bytes[7] = 0;
	out.bytes[8] = 0x80;
	for (int ix = 9; ix < 16; ++ix) {
		out.bytes[ix] = known_hex(bb[0], bb[1]);
		bb += 2;
	}
}

hipError_t hip_getBasicProps(int dev, BasicProps *bp, int fix_guid_magic){

	if (dev < 0 || dev >= (int)hip_platforms.size()) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: hip_getBasicProps(%d) Index out of range!\n", dev);
		return hipErrorAssert;
	}

	const hipDeviceProp_t & dev_props = hip_platforms[dev];

	bp->hipDetection = HIP_LIB_MAJOR_VERSION; // what version of hip (ROCM) libraries we used to get properties.

	if (is_null_uuid(dev_props.uuid)) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: dev(%d) UUID is NULL-GUID, ignoring\n", dev);
	} else if (is_max_uuid(dev_props.uuid)) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: dev(%d) UUID is MAX-GUID, ignoring\n", dev);
	} else if (is_plausiable_uuid(dev_props.uuid)) {
		bp->setUUIDFromBuffer((const unsigned char *)dev_props.uuid.bytes);
	} else if (fix_guid_magic) {
		hipUUID uuid;
		fix_rocm6_uuid(uuid, dev_props.uuid, fix_guid_magic);
		bp->setUUIDFromBuffer((const unsigned char *)uuid.bytes);
	} else {
		std::string bad;
		bad.assign(dev_props.uuid.bytes, sizeof(dev_props.uuid.bytes));
		print_error(MODE_DIAGNOSTIC_MSG, "diag: dev(%d) UUID 0x%s is not a GUID, ignoring\n", dev, bad.c_str());
	}
	bp->ccMajor = dev_props.major;
	bp->ccMinor = dev_props.minor;
	bp->totalGlobalMem = dev_props.totalGlobalMem;
	bp->clockRate = dev_props.clockRate;
	bp->multiProcessorCount = dev_props.multiProcessorCount;
	bp->ECCEnabled = dev_props.ECCEnabled;

	if (dev_props.pciBusID || dev_props.pciDeviceID) {
		snprintf(bp->pciId, sizeof(bp->pciId), "%04X:%02X:%02X.0",
			dev_props.pciDomainID, dev_props.pciBusID, dev_props.pciDeviceID);
	}

	// round trip the dev_props name though a std::string to force null termination
	std::string name; name.assign(dev_props.name, sizeof(dev_props.name));
	bp->name = name.c_str();
	//bp->name = dev_props.gcnArchName; // original PR used this for name, not sure why

	bp->xNACK = dev_props.unifiedAddressing;
	bp->warpSize = dev_props.warpSize;
	if (hip.hipDriverGetVersion) {
		hip.hipDriverGetVersion(& bp->driverVersion);
	}

	//print_error(MODE_DIAGNOSTIC_MSG, "diag: hip_getBasicProps(%d) %s\n", dev, bp->uuid.c_str());
	return hipSuccess;
}

