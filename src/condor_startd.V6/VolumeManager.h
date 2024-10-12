
#ifndef __VOLUME_MANAGER_H_
#define __VOLUME_MANAGER_H_

#include <string>
#include <condor_config.h>

const int VOLUME_MANAGER_TIMEOUT = 120;

class CondorError;
class Env;

class VolumeManager {

public:

    VolumeManager();
    VolumeManager(const VolumeManager&) = delete;
    ~VolumeManager();

    void UpdateStarterEnv(Env &env, const std::string & lv_name, long long disk_kb, bool encrypt);
    // See RemoveLV() for exit codes | is_encrypted: -1=Unknown, 0=false, 1=true
    int  CleanupLV(const std::string &lv_name, CondorError &err, int is_encrypted=-1);
    bool CleanupLVs();
    bool GetPoolSize(uint64_t &used_bytes, uint64_t &total_bytes, CondorError &err);
    static bool DetectLVM() {
        return param_boolean("STARTD_ENFORCE_DISK_LIMITS", false) && is_enabled();
    }

#ifdef LINUX
    static bool is_enabled() { return true; }
    bool IsSetup();

    inline std::string GetVG() const { return m_volume_group_name; }
    inline std::string GetPool() const { return m_pool_lv_name; }
    inline std::string GetLoopDev() const { return m_loopdev_name; }
    inline std::string GetLoopFile() const { return m_loopback_filename; }
    inline int GetTimeout() const { return m_cmd_timeout; }
    inline bool IsThin() const { return m_use_thin_provision; }

    class Handle {
    public:
        Handle() = delete;
        Handle(const Handle&) = delete;
        Handle(const std::string &mountpoint, const std::string &volume, const std::string &pool,
               const std::string &vg_name, uint64_t size_kb, bool encrypt, CondorError &err);
        ~Handle();
        inline bool HasInfo() const { return !m_volume.empty() && !m_vg_name.empty(); }
        inline std::string GetVG() const { return m_vg_name; }
        inline std::string GetLVName() const { return m_volume; }
        inline std::string GetPool() const { return m_thinpool; }
        inline std::string GetMount() const { return m_mountpoint; }
        inline int GetTimeout() const { return m_timeout; }
        inline bool IsThin() const { return m_thin; }
        inline bool IsEncrypted() const { return m_encrypt; }
        inline int SetPermission(int perms) { TemporaryPrivSentry sentry(PRIV_ROOT); return chmod(m_mountpoint.c_str(), perms); }

    private:
        std::string m_mountpoint;
        std::string m_volume;
        std::string m_vg_name;
        std::string m_thinpool;
        int m_timeout{VOLUME_MANAGER_TIMEOUT};
        bool m_thin{true};
        bool m_encrypt{false};
    };

    static bool GetVolumeUsage(const VolumeManager::Handle* handle, filesize_t &used_bytes, size_t &numFiles, bool &out_of_space, CondorError &err);

private:
    static bool MountFilesystem(const std::string &device_path, const std::string &mountpoint, CondorError &err, int timeout);
    static std::string CreateLoopback(const std::string &backing_filename, uint64_t size_kb, CondorError &err, int timeout);
    static bool CreateThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err, int timeout);
    static bool CreateLV(const VolumeManager::Handle &handle, uint64_t size_kb, CondorError &err);
    static bool CreateVG(const std::string &vg_name, const std::string &device, CondorError &err, int timeout);
    static bool CreatePV(const std::string &device, CondorError &err, int timeout);
    static bool CreateFilesystem(const std::string &label, const std::string &device_path, CondorError &err, int timeout);
    static bool EncryptLV(const std::string &lv_name, const std::string &vg_name, CondorError &err, int timeout);
    static void RemoveLostAndFound(const std::string& mountpoint);
    static bool RemoveLVEncryption(const std::string &lv_name, const std::string &vg_name, CondorError &err, int timeout);
    static bool CleanupAllDevices(const VolumeManager &info, CondorError &err, bool cleanup_loopback = true);
    static bool UnmountFilesystem(const std::string &mountpoint, CondorError &err);
    /* RemoveLV() returns: 0 on success, <0 on failure, >0 on warnings (2 = LV not found) */
    static int  RemoveLV(const std::string &lv_name, const std::string &vg_name, CondorError &err, bool encrypted, int timeout);
    static bool RemoveVG(const std::string &vg_name, CondorError &err, int timeout);
    static bool RemovePV(const std::string &pv_name, CondorError &err, int timeout);
    static bool RemoveLoopDev(const std::string &loopdev_name, const std::string &backing_file, CondorError &err, int timeout);
    static bool GetPoolSize(const VolumeManager &info, uint64_t &used_bytes, uint64_t &total_bytes, CondorError &err);


    bool m_use_thin_provision{true};
    std::string m_pool_lv_name;
    std::string m_volume_group_name;
    std::string m_loopback_filename;
    std::string m_loopdev_name;
    int m_cmd_timeout{VOLUME_MANAGER_TIMEOUT};
#else
    static bool is_enabled() { return false; }
    bool IsSetup() { return false; };
#endif // LINUX
};


#endif
