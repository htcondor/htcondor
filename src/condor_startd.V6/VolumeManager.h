
#include <string>

class CondorError;
class Env;

class VolumeManager {

public:

    VolumeManager();
    VolumeManager(const VolumeManager&) = delete;
    ~VolumeManager();

    void UpdateStarterEnv(Env &env);
    bool CleanupSlot(const std::string &slot, CondorError &err);
    bool GetPoolSize(uint64_t &used, uint64_t &total, CondorError &err);
	//
	// Return the usage of a specific volume managed by HTCondor; returns false on
	// error and populates CondorError appropriately.
	//
	// `out_of_space` is set to True if the underlying thin pool is full (which indicates that
	// all writes to the volume are likely to block).
	//
    static bool GetThinVolumeUsage(const std::string &thin_volume_name, const std::string &pool, const std::string &vg_name, uint64_t &used_bytes, bool &out_of_space, CondorError &err);


    class Handle {
    public:
        Handle() = delete;
        Handle(const Handle&) = delete;
        Handle(const std::string &mountpoint, const std::string &volume, const std::string &pool, const std::string &vg_name, uint64_t size_kb, CondorError &err);
        ~Handle();

    private:
        std::string m_mountpoint;
        std::string m_volume;
        std::string m_vg_name;
    };

private:
    static bool MountFilesystem(const std::string &device_path, const std::string &mountpoint, CondorError &err);
    static std::string CreateLoopback(const std::string &backing_filename, uint64_t size_kb, CondorError &err);
    static bool CreateThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err);
    static bool CreateThinLV(uint64_t size_kb, const std::string &lv_name, const std::string &pool_name, const std::string &vg_name, CondorError &err);
    static bool CreateVG(const std::string &vg_name, const std::string &device, CondorError &err);
    static bool CreatePV(const std::string &device, CondorError &err);
    static bool CreateFilesystem(const std::string &label, const std::string &device_path, CondorError &err);
    static bool EncryptThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err);
    static bool RemoveEncryptedThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err);
    static bool CleanupAllDevices(const std::string &loopdev, const std::string &pv_name, const std::string &vg_name, const std::string &pool_name, CondorError &err);
    static bool UnmountFilesystem(const std::string &mountpoint, CondorError &err);
    static bool RemoveLV(const std::string &lv_name, const std::string &vg_name, CondorError &err);
    static bool RemoveVG(const std::string &lv_name, CondorError &err);
    static bool RemovePV(const std::string &lv_name, CondorError &err);
    static bool RemoveLoopDev(const std::string &lv_name, CondorError &err);
    static std::vector<std::string> ListPoolLVs(const std::string &pool_name, CondorError &err);
    static bool GetPoolSize(const std::string &pool_name, const std::string &vg_name, uint64_t &used, uint64_t &total, CondorError &err);


    bool m_encrypt{false};
    std::string m_volume_name;
    std::string m_volume_group_name;
    std::string m_loopback_filename;
    std::string m_loopdev_name;
    std::string m_pv_name;
    std::string m_vg_name;
    std::string m_pool_name;
};
