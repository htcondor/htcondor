
#include <condor_common.h>
#include <CondorError.h>
#include <condor_debug.h>
#include <condor_crypt.h>
#include <my_popen.h>
#include <condor_uid.h>
#include <condor_attributes.h>
#include <subsystem_info.h>
#include "VolumeManager.h"

#ifdef LINUX

#include <sys/mount.h>
#include <sys/statvfs.h>
#include "directory.h"


#include <sstream>

// 256-bit encryption
static const int KEY_SIZE = 32;
static const std::string CONDOR_LV_TAG = "htcondor_lv";
static const std::string LV_ENCRYPTED_TAG = "lv_is_encrypted";
static const std::string ENCRYPT_SUFFIX = "-enc";
static const char* LVM_REPORT_DELIM = "\x1F";

struct LVMReportFilter {
    LVMReportFilter() = delete;
    LVMReportFilter(const std::string& vg) : volume_group(vg) {};
    LVMReportFilter(const std::string& vg, const std::string& pool) : volume_group(vg), thinpool(pool) {};

    LVMReportFilter& AddLV(const std::string& lv) { lv_names.insert(lv); return *this; }
    LVMReportFilter& SkipThinpool() { ignore_thinpool = true; return *this; }

    std::set<std::string> lv_names{};
    std::string volume_group{};
    std::string thinpool{};
    bool ignore_thinpool{false};
};

struct LVMReportItem {
    LVMReportItem(bool lv) : is_lv(lv) {};
    std::string name{}; // Volume Group or LV name
    std::string size{}; // Total size bytes of object
    std::string data{}; // VG free or LV data % used
    bool is_lv{false};  // Is this item an LV or Volume Group
    bool encrypted{false}; // Only normal LVs can be encrypted (non-thinpool)
};

static std::string DevicePath(const std::string& vg, const std::string& lv, bool add_suffix=false) {
    std::string suffix = add_suffix ? ENCRYPT_SUFFIX : "";
    return std::string("/dev/mapper/") + vg + "-" + lv + suffix;
}

VolumeManager::VolumeManager()
{
    std::string volume_group_name, pool_name;
    m_use_thin_provision = param_boolean("LVM_USE_THIN_PROVISIONING", false);
    m_cmd_timeout = param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT);

    if ( ! param(volume_group_name, "LVM_VOLUME_GROUP_NAME")) {
        param(volume_group_name, "THINPOOL_VOLUME_GROUP_NAME");
        if ( ! volume_group_name.empty())
            dprintf(D_ALWAYS, "THINPOOL_VOLUME_GROUP_NAME is depreacted. Please replace with LVM_VOLUME_GROUP_NAME\n");
    }

    if ( ! param(pool_name, "LVM_THINPOOL_NAME")) {
        param(pool_name, "THINPOOL_NAME");
        if ( ! pool_name.empty())
            dprintf(D_ALWAYS, "THINPOOL_NAME is deprecated. Please replace with LVM_THINPOOL_NAME\n");
    }


    if (volume_group_name.empty() || (m_use_thin_provision && pool_name.empty())) {
        bool lvm_setup_successful = false;
        uint64_t size_mb = param_integer("LVM_BACKING_FILE_SIZE_MB", 10240);
        auto lname = get_mySubSystem()->getLocalName();
        CondorError err;

        param(m_loopback_filename, "LVM_BACKING_FILE");
        dprintf(D_ALWAYS, "StartD will enforce disk limits using loopback file %s.\n", m_loopback_filename.c_str());
        m_loopdev_name = CreateLoopback(m_loopback_filename, size_mb*1024, err, m_cmd_timeout);
        if (m_loopdev_name.empty()) {
            dprintf(D_ALWAYS, "Failed to allocate loopback device: %s\n", err.getFullText().c_str());
            goto end_lvm_setup;
        }

        m_volume_group_name = "condor_" + (lname ? (std::string(lname) + "_") : "") + get_mySubSystem()->getName();
        if (m_use_thin_provision) { m_pool_lv_name = m_volume_group_name + "_THINPOOL"; }

            // Preemptively clean-up condor-owned devices.
        CleanupAllDevices(*this, err, false);
        err.clear();

        if ( ! CreatePV(m_loopdev_name, err, m_cmd_timeout)) {
            dprintf(D_ALWAYS, "Failed to create LVM physical volume for loopback device: %s\n",
                    err.getFullText().c_str());
            m_volume_group_name.clear();
            m_pool_lv_name.clear();
            goto end_lvm_setup;
        }

        if ( ! CreateVG(m_volume_group_name, m_loopdev_name, err, m_cmd_timeout)) {
            dprintf(D_ALWAYS, "Failed to create LVM volume group for loopback device %s: %s\n",
                    m_loopdev_name.c_str(), err.getFullText().c_str());
            m_volume_group_name.clear();
            m_pool_lv_name.clear();
            goto end_lvm_setup;
        }

        if (m_use_thin_provision) {
            if ( ! CreateThinPool(m_pool_lv_name, m_volume_group_name, err, m_cmd_timeout)) {
                dprintf(D_ALWAYS, "Failed to create LVM thinpool logical volume for loopback device %s: %s\n",
                        m_loopdev_name.c_str(), err.getFullText().c_str());
                m_pool_lv_name.clear();
                goto end_lvm_setup;
            }
        }
        lvm_setup_successful = true;

end_lvm_setup:
        ASSERT(lvm_setup_successful);

    } else {
        m_volume_group_name = volume_group_name;
        if (m_use_thin_provision) { m_pool_lv_name = pool_name; }
    }

    std::string msg_end = "\n";
    if (m_use_thin_provision) {
        formatstr(msg_end, " | Backing Thinpool LV: %s\n", m_pool_lv_name.c_str());
    }
    dprintf(D_ALWAYS, "StartD disk enforcement using Volume Group: %s%s",
            m_volume_group_name.c_str(), msg_end.c_str());
}

VolumeManager::~VolumeManager()
{
    if ( ! m_volume_group_name.empty()) { CleanupLVs(); }
    if ( ! m_loopback_filename.empty()) {
        CondorError err;
        if ( ! CleanupAllDevices(*this, err)){
            dprintf(D_ALWAYS, "Failed to cleanup volume manager devices: %s\n", err.getFullText().c_str());
        }
    }
}


VolumeManager::Handle::Handle(const std::string &vg_name, const std::string &volume, const char* pool, bool encrypt) {
    m_timeout = param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT);
    m_volume = volume;
    m_vg_name = vg_name;
    if (pool) {
        m_thinpool = pool;
        m_thin = true;
    };
    m_encrypt = encrypt;
}


VolumeManager::Handle::~Handle() {
    CondorError err;
    if ( ! m_cleaned_up && ! this->CleanupLV(err)) {
        dprintf(D_ALWAYS, "Failed to cleanup LV in Handle::~Handle(): %s\n", err.getFullText().c_str());
    }
}


bool
VolumeManager::Handle::SetupLV(const std::string& mountpoint, uint64_t size_kb, bool hide_mount, CondorError& err) {
    m_cleaned_up = false;

    // Add extra LV size if using thin provisioning
    if (m_thin) {
        int extra_volume_size_mb = param_integer("LVM_THIN_LV_EXTRA_SIZE_MB", 2000);
        size_kb += 1024LL * extra_volume_size_mb;
    }

    // Create LV
    if ( ! VolumeManager::CreateLV(*this, size_kb, err)) {
        return false;
    }

    // Create filesystem in LV
    std::string device_path = DevicePath(m_vg_name, m_volume, m_encrypt);
    if ( ! VolumeManager::CreateFilesystem(m_volume, device_path, err, m_timeout)) {
        return false;
    }

    // Mount LV to working directory
    if ( ! VolumeManager::MountFilesystem(device_path, mountpoint, err, hide_mount)) {
        return false;
    }

    // Store mount point
    m_mountpoint = mountpoint;
    // Remove lost+found directory (best effort)
    VolumeManager::RemoveLostAndFound(mountpoint);
    return true;
}


bool
VolumeManager::Handle::CleanupLV(CondorError& err) {
    // Unmount if needed
    if ( ! m_mountpoint.empty()) {
        if ( ! UnmountFilesystem(m_mountpoint, err)) {
            return false;
        }
        m_mountpoint.clear();
    }

    // Remove the LV
    if ( ! m_volume.empty() && RemoveLV(m_volume, m_vg_name, err, m_encrypt, m_timeout) < 0) {
        return false;
    }

    m_cleaned_up = true;

    return true;
}


int
VolumeManager::CleanupLV(const std::string &lv_name, CondorError &err, int is_encrypted)
{
    dprintf(D_FULLDEBUG, "StartD is cleaning up logical volume %s.\n", lv_name.c_str());
    if (!lv_name.empty() && !m_volume_group_name.empty()) {
        if (is_encrypted == -1 /*Unknown*/) {
            struct stat statbuf;
            int ret = stat(DevicePath(m_volume_group_name, lv_name, true).c_str(), &statbuf);
            is_encrypted = (int)(ret == 0);
        }
        return RemoveLV(lv_name, m_volume_group_name, err, (bool)is_encrypted, m_cmd_timeout);
    }
    return 0;
}


static bool
isHideMountCompatible(const ClassAd& ad) {
    // NOTE: If adding an incompatibilty then make sure Startd doesn't advertise
    //       the associated capability (or make the hide mount checking more robust)

    // Pure docker universe jobs are incompatible with hide mount
    bool isDockerJob = false;
    if (ad.LookupBool(ATTR_WANT_DOCKER, isDockerJob) && isDockerJob) {
        return false;
    }

    // All good
    return true;
}


bool
VolumeManager::CheckHideMount(ClassAd* ad, bool& hide_mount) {
    int check_hide_mnt = VolumeManager::GetHideMount();
    bool compatible = true;
    switch (check_hide_mnt) {
        // Never Hide Mount
        case LVM_DONT_HIDE_MOUNT:
            hide_mount = false;
            break;
        // Always Hide mount
        case LVM_ALWAYS_HIDE_MOUNT:
            hide_mount = true;
            if (ad) {
                if ( ! isHideMountCompatible(*ad)) {
                    // Job is incompatible with hide mounts
                    dprintf(D_ERROR, "Job is incompatible with LVM_HIDE_MOUNT = True.\n");
                    compatible = false;
                }
            } else {
                // Ad is NULL, we can't check incompatibility so don't continue
                dprintf(D_ERROR, "Unable to verify job compatibility with LVM_HIDE_MOUNT = True.\n");
                compatible = false;
            }
            break;
        // Auto hide mount (Best effort): Check job ad for incompatibilities
        case LVM_AUTO_HIDE_MOUNT:
            if (ad) {
                hide_mount = isHideMountCompatible(*ad);
            } else {
                // Ad is NULL so assume we can't hide mount
                dprintf(D_FULLDEBUG, "Unable to check job ad for LVM_HIDE_MOUNT = AUTO incompatability.\n");
                hide_mount = false;
            }
            break;
        default:
            // We should never get here!
            dprintf(D_ERROR, "Unknown configured LVM hide mount option: %d\n", check_hide_mnt);
            compatible = false;
    }

    return compatible;
}


bool
VolumeManager::MountFilesystem(const std::string &device_path, const std::string &mountpoint, CondorError &err, bool hide_mount) {
    TemporaryPrivSentry sentry(PRIV_ROOT);

    if (hide_mount) {
        dprintf(D_FULLDEBUG, "Volume Manager creating mount namespace...\n");
        if (-1 == unshare(CLONE_NEWNS)) {
            err.pushf("VolumeManager", 14, "Failed to create new mount namespace for starter (errno=%d): %s",
                      errno, strerror(errno));
            return false;
        }
        if (-1 == mount("", "/", "dontcare", MS_REC|MS_SLAVE, nullptr)) {
            err.pushf("VolumeManager", 15, "Failed to unshare the mount namespace (errno=%d): %s",
                      errno, strerror(errno));
            return false;
        }
    }

    dprintf(D_FULLDEBUG, "Mounting device %s to %s\n", device_path.c_str(), mountpoint.c_str());

    if (mount(device_path.c_str(), mountpoint.c_str(), "ext4", MS_NOATIME|MS_NODEV|MS_NOSUID, "barrier=0,discard") != 0) {
        err.pushf("VolumeManager", 13, "Failed to mount new filesystem %s for job (errno=%d): %s\n",
                  mountpoint.c_str(), errno, strerror(errno));
        return false;
    }

    if (-1 == chown(mountpoint.c_str(), get_user_uid(), get_user_gid())) {
        err.pushf("VolumeManager", 14, "Failed to chown the new execute mount to user (errno=%d): %s",
                  errno, strerror(errno));
        return false;
    }

    return true;
}


bool
VolumeManager::CreateFilesystem(const std::string &label, const std::string &devname, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Creating new filesystem for device %s.\n", devname.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);

    ArgList args;
    args.AppendArg("mke2fs");
    args.AppendArg("-t");
    args.AppendArg("ext4");
    args.AppendArg("-O");
    args.AppendArg("^has_journal");
    args.AppendArg("-E");
    args.AppendArg("lazy_itable_init=0");
    args.AppendArg("-m");
    args.AppendArg("0");
    args.AppendArg("-L");
    args.AppendArg(label);
    args.AppendArg(devname);
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> mke2fs_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 13, "Failed to create new ext4 filesystem for job (exit status %d): %s\n",
                  exit_status, mke2fs_output ? mke2fs_output.get(): "(no command output)");
        return false;
    }
    return true;
}


void
VolumeManager::RemoveLostAndFound(const std::string& mountpoint) {
    TemporaryPrivSentry sentry(PRIV_ROOT);

    // Remove lost+found directory from new filesystem
    std::string lost_n_found;
    dircat(mountpoint.c_str(), "lost+found", lost_n_found);
    if (rmdir(lost_n_found.c_str())) {
        dprintf(D_ERROR, "Failed to remove 'lost+found' directory from per job filesystem (%d): %s\n",
                         errno, strerror(errno));
    }
}


std::string
VolumeManager::CreateLoopback(const std::string &filename, uint64_t size_kb, CondorError &err, int timeout)
{
    if (filename.empty()) {
        err.push("VolumeManager", 1, "Loopback filename is empty");
        return "";
    }
    if ( ! size_kb) {
        err.push("VolumeManager", 2, "Loopback filesystem size is zero");
        return "";
    }

    TemporaryPrivSentry sentry(PRIV_ROOT);

    struct stat backing_stat;
    bool existing_device = false;
    std::string output_devname;
    if (0 == stat(filename.c_str(), &backing_stat)) {
        ArgList args;
        args.AppendArg("losetup");
        args.AppendArg("--list");
        args.AppendArg("--output");
        args.AppendArg("name,back-file");
        std::string cmdDisplay;
        args.GetArgsStringForLogging(cmdDisplay);
        dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
        int exit_status;
        std::unique_ptr<char, decltype(free)*> losetup_output(
            run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
            free);
        if (exit_status) {
            err.pushf("VolumeManager", 3, "Failed to list current loopback devices (exit status %d): %s\n",
                      exit_status, losetup_output ? losetup_output.get(): "(no command output)");
            return "";
        }

        std::stringstream ss(losetup_output.get());
        bool found_header = false;
        for (std::string line; std::getline(ss, line); ) {
            if ( ! found_header) {
                found_header = true;
                continue;
            }
            std::stringstream ss2(line);
            std::string devname, backing;
            for (std::string word; std::getline(ss2, word, ' '); ) {
                if (word.empty()) {continue;}
                if (devname.empty()) devname = word;
                else if (backing.empty()) backing = word;
            }
            if (backing.empty()) {
                continue;
            }

            struct stat cur_backing_stat;
            if (0 == stat(backing.c_str(), &cur_backing_stat) &&
                cur_backing_stat.st_dev == backing_stat.st_dev &&
                cur_backing_stat.st_ino == backing_stat.st_ino)
            {
                existing_device = true;
                output_devname = devname;
                break;
            }
        }
    } else {
        int fd = safe_create_fail_if_exists(filename.c_str(), O_WRONLY, 0600);
        if (fd == -1) {
            err.pushf("VolumeManager", 4, "Failed to create loopback file %s: %s (errno=%d)",
                filename.c_str(), strerror(errno), errno);
            return "";
        }
        int alloc_error = posix_fallocate(fd, 0, static_cast<off_t>(size_kb*1.02)*1024);
        int r = fstat(fd, &backing_stat);
        if (r < 0) {
            err.pushf("VolumeManager", 4, "Failed to stat  %s: %s (errno=%d)",
                filename.c_str(), strerror(errno), errno);
            close(fd);
            return "";
        }
        close(fd);
        if (alloc_error) {
            err.pushf("VolumeManager", 5, "Failed to allocate space for data in %s: %s (errno=%d)",
                filename.c_str(), strerror(alloc_error), alloc_error);
            return "";
        }
    }


        // TODO: Would be friendlier to unmount everything and resize instead of failing
    if (backing_stat.st_size < static_cast<off_t>(size_kb*1.02)*1024) {
        err.pushf("VolumeManager", 6, "Pre-existing loopback file is too small (is %g, asking for %g\n)",
                  (double) backing_stat.st_size, (size_kb * 1.02) * 1024);
        return "";
    }

    if ( ! existing_device) {
        dprintf(D_FULLDEBUG, "Loopback device does not already exist; creating for file %s.\n", filename.c_str());
        ArgList args;
        args.AppendArg("losetup");
        args.AppendArg("--find");
        args.AppendArg("--show");
        args.AppendArg(filename.c_str());
        std::string cmdDisplay;
        args.GetArgsStringForLogging(cmdDisplay);
        dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
        int exit_status;
        std::unique_ptr<char, decltype(free)*> losetup_output(
            run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
            free);
        if (exit_status) {
            err.pushf("VolumeManager", 7, "Failed to setup loopback device from backing file %s (exit status=%d): %s",
                      filename.c_str(), exit_status, losetup_output ? losetup_output.get() : "(no output)");
            return "";
        }
        output_devname = losetup_output ? losetup_output.get() : "";
        chomp(output_devname);
        if (output_devname.empty()) {
            err.pushf("VolumeManager", 8, "Creation of loopback device returned empty output");
            return "";
        }
    } else {
        dprintf(D_FULLDEBUG, "Loopback device already exists; re-using %s.\n", output_devname.c_str());
    }
    return output_devname;
}


bool
VolumeManager::CreatePV(const std::string &devname, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Creating LVM metadata for physical volume %s.\n", devname.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("pvcreate");
    args.AppendArg(devname);
    int exit_status;
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    std::unique_ptr<char, decltype(free)*> pvcreate_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM metadata for physical volume %s (exit status %d): %s",
                  devname.c_str(), exit_status, pvcreate_output ? pvcreate_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::CreateVG(const std::string &vg_name, const std::string &devname, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Creating LVM volume group %s for physical volume %s.\n", vg_name.c_str(), devname.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("vgcreate");
    args.AppendArg("--autobackup");
    args.AppendArg("n");
    args.AppendArg(vg_name);
    args.AppendArg(devname);
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> vgcreate_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM volume group for physical volume %s (exit status %d): %s",
                  devname.c_str(), exit_status, vgcreate_output ? vgcreate_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::CreateThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Creating LVM thin pool logical volume %s for volume group %s.\n", lv_name.c_str(), vg_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("lvcreate");
    args.AppendArg("--autobackup");
    args.AppendArg("n");
    args.AppendArg("--type");
    args.AppendArg("thin-pool");
    args.AppendArg("-l");
    args.AppendArg("100\%FREE");
    args.AppendArg(vg_name);
    args.AppendArg("-n");
    args.AppendArg(lv_name);
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> lvcreate_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM thin logical volume pool %s for volume group %s (exit status %d): %s",
                  lv_name.c_str(), vg_name.c_str(), exit_status, lvcreate_output ? lvcreate_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::CreateLV(const VolumeManager::Handle& handle, uint64_t size_kb, CondorError& err)
{
    std::string vg_name = handle.GetVG();
    std::string lv_name = handle.GetLVName();

    dprintf(D_FULLDEBUG, "Creating LVM logical volume %s for volume group %s.\n", lv_name.c_str(), vg_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    // lvcreate -V 1G -T test_vg/condor_thinpool -n condor_slot_1
    args.AppendArg("lvcreate");
    args.AppendArg("--autobackup");
    args.AppendArg("n");
    args.AppendArg("-n");
    args.AppendArg(lv_name);
    args.AppendArg("--addtag");
    args.AppendArg(CONDOR_LV_TAG);
    if (handle.IsEncrypted()) {
        args.AppendArg("--addtag");
        args.AppendArg(LV_ENCRYPTED_TAG);
    }
    std::string size;
    formatstr(size, "%luk", size_kb);
    if (handle.IsThin()) {
        args.AppendArg("-V");
        args.AppendArg(size);
        args.AppendArg("-T");
        args.AppendArg(vg_name + "/" + handle.GetPool());
    } else {
        args.AppendArg("--yes");
        args.AppendArg("-L");
        args.AppendArg(size);
        args.AppendArg(vg_name);
    }
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> lvcreate_output(
        run_command(handle.GetTimeout(), args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM logical volume %s for volume group %s (exit status %d): %s",
                  lv_name.c_str(), vg_name.c_str(), exit_status, lvcreate_output ? lvcreate_output.get() : "(no output)");
        return false;
    }

    if (handle.IsEncrypted()) { return EncryptLV(lv_name, vg_name, err, handle.GetTimeout()); }
    return true;
}


bool
VolumeManager::EncryptLV(const std::string &lv_name, const std::string &vg_name, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Encrypting LVM logical volume %s", lv_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);

    char crypto_key[] = "/dev/shm/cryptokeyXXXXXX";
    int fd = mkstemp(crypto_key);
    if (-1 == fd) {
        err.pushf("VolumeManager", 17, "Failed to create temporary crypto key at %s: %s (errno=%d)",
            crypto_key, strerror(errno), errno);
        return false;
    }

    std::unique_ptr<unsigned char, decltype(free)*> key(Condor_Crypt_Base::randomKey(KEY_SIZE),free);
    if ( ! key) {
        err.push("VolumeManager", 17, "Failed to create crypto key");
        close(fd);
        unlink(crypto_key);
        return false;
    }

    if (KEY_SIZE != full_write(fd, key.get(), KEY_SIZE)) {
        err.pushf("VolumeManager", 17, "Failed to write crypto key: %s (errno=%d)",
            strerror(errno), errno);
        close(fd);
        unlink(crypto_key);
        return false;
    }
    close(fd);

    ArgList args;
    args.AppendArg("cryptsetup");
    args.AppendArg("open");
    args.AppendArg("--type");
    args.AppendArg("plain");
    args.AppendArg("--key-file");
    args.AppendArg(crypto_key);
    args.AppendArg(DevicePath(vg_name, lv_name));
    args.AppendArg(vg_name + "-" + lv_name + ENCRYPT_SUFFIX);
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> cryptsetup_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    unlink(crypto_key);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to setup encrypted volume on logical volume %s (exit status %d): %s",
                  lv_name.c_str(), exit_status, cryptsetup_output ? cryptsetup_output.get() : "(no output)");
        return false;
    }

    return true;
}


bool
VolumeManager::UnmountFilesystem(const std::string &mountpoint, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Unmounting filesystem at %s.\n", mountpoint.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);

    std::unique_ptr<char, decltype(free)*> cwd(get_current_dir_name(),free);
    if (-1 == chdir("/")) {
        err.pushf("VolumeManager", 16, "Failed to change directory: %s (errno=%d)",
                  strerror(errno), errno);
    }

    bool unmounted = true;;
    if (-1 == umount2(mountpoint.c_str(), MNT_DETACH)) {
        unmounted = false;
        err.pushf("VolumeManager", 17, "Failed to unmount %s: %s (errno=%d)",
                  mountpoint.c_str(), strerror(errno), errno);
    }

    if (cwd) {
        if (-1 == chdir(cwd.get())) {
            err.pushf("VolumeManager", 16, "Failed to change directory: %s (errno=%d)",
                      strerror(errno), errno);
        }
    }

    return unmounted;
}


bool
VolumeManager::RemoveLVEncryption(const std::string &lv_name, const std::string &vg_name, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Removing volume %s encryption.\n", lv_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("cryptsetup");
    args.AppendArg("close");
    args.AppendArg(vg_name + "-" + lv_name + ENCRYPT_SUFFIX);
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> cryptsetup_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete encrypted volume %s (exit status %d): %s",
            lv_name.c_str(), exit_status, cryptsetup_output ? cryptsetup_output.get() : "(no output)");
        return false;
    }
    return true;
}


int // RemoveLV() returns: 0 on success, <0 on failure, >0 on warnings (2 = LV not found)
VolumeManager::RemoveLV(const std::string &lv_name, const std::string &vg_name, CondorError &err, bool encrypted, int timeout)
{
    TemporaryPrivSentry sentry(PRIV_ROOT);

    // If a crash or restart left the volume mounted, unmount it now
    // Format of /proc/mounts is
    // device-name mount-point fstype other_stuff
    // Find the matching device-name field, so we can umount the mount-point
    FILE *f = fopen("/proc/self/mounts", "r");
    if (f == nullptr) {
        dprintf(D_ALWAYS, "VolumeManager::RemoveLV error opening /proc/self/mounts: %s\n", strerror(errno));
    } else {
        char dev[PATH_MAX];
        char mnt[PATH_MAX];
        char dummy[PATH_MAX];

        std::string per_slot_device = DevicePath(vg_name, lv_name, encrypted);
        while (fscanf(f, "%s %s %s\n", dev, mnt, dummy) > 0) {
            if (strcmp(dev, per_slot_device.c_str()) == MATCH) {
                dprintf(D_ALWAYS, "VolumeManager::RemoveLV found leftover mount from device %s on path %s, umounting\n",
                        dev, mnt);
                int r = umount(mnt);
                if (r != 0) {
                    dprintf(D_ALWAYS, "VolumeManager::RemoveLV error umounting %s %s\n", mnt, strerror(errno));
                    fclose(f);
                    return -1;
                }
                break;
            }
        }
        fclose(f);
    }

    if (encrypted) { (void)RemoveLVEncryption(lv_name, vg_name, err, timeout); }

    dprintf(D_FULLDEBUG, "Removing logical volume %s.\n", lv_name.c_str());

    ArgList args;
    args.AppendArg("lvremove");
    args.AppendArg("--autobackup");
    args.AppendArg("n");
    args.AppendArg(vg_name + "/" + lv_name);
    args.AppendArg("--yes");
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> lvremove_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        std::string cmdErr = lvremove_output ? lvremove_output.get() : "(no output)";
        err.pushf("VolumeManager", 12, "Failed to delete logical volume %s (exit status %d): %s",
                  lv_name.c_str(), exit_status, cmdErr.c_str());
        // Distinguish between LV not found (2) and other errors (-1)
        return cmdErr.find("Failed to find logical volume") != std::string::npos ? 2 : -1;
    }
    return 0;
}


bool
VolumeManager::RemoveVG(const std::string &vg_name, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Removing LVM volume group %s.\n", vg_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("vgremove");
    args.AppendArg("--autobackup");
    args.AppendArg("n");
    args.AppendArg(vg_name);
    args.AppendArg("--yes");
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> vgremove_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete volume group %s (exit status %d): %s",
                  vg_name.c_str(), exit_status, vgremove_output ? vgremove_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::RemovePV(const std::string &pv_name, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Removing physical volume %s.\n", pv_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("pvremove");
    args.AppendArg(pv_name);
    args.AppendArg("--yes");
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> pvremove_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete LVM physical volume %s (exit status %d): %s",
                  pv_name.c_str(), exit_status, pvremove_output ? pvremove_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::RemoveLoopDev(const std::string &loopdev_name, const std::string &backing_file, CondorError &err, int timeout)
{
    dprintf(D_FULLDEBUG, "Removing loopback device %s.\n", loopdev_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("losetup");
    args.AppendArg("-d");
    args.AppendArg(loopdev_name);
    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());
    int exit_status;
    std::unique_ptr<char, decltype(free)*> losetup_output(
        run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status),
        free);
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete loopback device %s (exit status %d): %s",
                  loopdev_name.c_str(), exit_status, losetup_output ? losetup_output.get() : "(no output)");
        return false;
    }
    dprintf(D_FULLDEBUG, "Removing backing loopback file %s\n", backing_file.c_str());
    if (remove(backing_file.c_str()) != 0) {
        err.pushf("VolumeManager", 12, "Failed to delete loopback file %s: Error (%d) %s",
                  backing_file.c_str(), errno, strerror(errno));
        return false;
    }
    return true;
}

static bool
extractReportVal(const std::string& data, std::string& value) { // data: LM2_Option1='value'
    size_t begin = data.find("'");
    size_t end = data.rfind("'");
    if (begin == end || begin == std::string::npos || end == std::string::npos) {
        dprintf(D_ERROR, "Error: Malformed LVM Report information [%s]\n", data.c_str());
        return false;
    }
    int len = end-begin-1;
    value = len <= 0 ? "" : data.substr(begin+1, end-begin-1);
    return true;
}

static bool
getLVMReport(std::vector<LVMReportItem>& results, CondorError &err, const LVMReportFilter& filter, int timeout, bool query_lvs=true)
{
    std::string exe = query_lvs ? "lvs" : "vgs";
    std::string options = query_lvs ? "vg_name,lv_name,pool_lv,lv_tags,lv_size,data_percent" : "vg_name,vg_size,vg_free";

    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg(exe);
    args.AppendArg("--noheadings");
    args.AppendArg("--nameprefixes");
    args.AppendArg("--separator");
    args.AppendArg(LVM_REPORT_DELIM);
    args.AppendArg("--units");
    args.AppendArg("b");
    args.AppendArg("--options");
    args.AppendArg(options);

    // Query Specific items (Selected LVs or associated VG)
    if (query_lvs) {
        for (const auto& lv : filter.lv_names) {
            args.AppendArg(filter.volume_group + "/" + lv);
        }
    } else {
        args.AppendArg(filter.volume_group);
    }

    std::string cmdDisplay;
    args.GetArgsStringForLogging(cmdDisplay);
    dprintf(D_FULLDEBUG,"Running: %s\n",cmdDisplay.c_str());

    int exit_status;
    auto_free_ptr report = run_command(timeout, args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status);

    if (exit_status) {
        err.pushf("VolumeManager", 9, "Failed to list %s (exit status=%d): %s",
                  query_lvs ? "logical volumes" : "volume groups",
                  exit_status, report ? report.ptr() : "(no output)");
        return false;
    }

    if ( ! report) { // Empty is not an error
        dprintf(D_FULLDEBUG, "%s yielded no %s", exe.c_str(), query_lvs ? "logical volumes" : "volume groups");
        return true;
    }

    size_t expected_item_count = query_lvs ? 6 : 3;
    for (const auto& line : StringTokenIterator(report.ptr(), "\n")) { // line: LM2_Option1='value'@LVM_Option2='value'@LVM2_Option3=''
        LVMReportItem item(query_lvs);
        auto info = split(line, LVM_REPORT_DELIM);
        if (info.size() != expected_item_count) {
            dprintf(D_ERROR, "Error: Unexpected number of items in LVM report line: %s\n", line.c_str());
            continue;
        }

        if (query_lvs) {
            std::string vg, pool, tags;
            if (!extractReportVal(info[0], vg) || vg != filter.volume_group ||
                !extractReportVal(info[1], item.name) || (!filter.lv_names.empty() && !filter.lv_names.contains(item.name)) ||
                !extractReportVal(info[2], pool) || (filter.thinpool != item.name && pool != filter.thinpool) || (filter.ignore_thinpool && filter.thinpool == item.name) ||
                !extractReportVal(info[3], tags) || (filter.thinpool != item.name && tags.find(CONDOR_LV_TAG) == std::string::npos) ||
                !extractReportVal(info[4], item.size) || !extractReportVal(info[5], item.data))
            {
                continue;
            }
            item.encrypted = tags.find(LV_ENCRYPTED_TAG) != std::string::npos;
            results.push_back(std::move(item));

        } else {
            if (!extractReportVal(info[0], item.name) || item.name != filter.volume_group ||
                !extractReportVal(info[1], item.size) || !extractReportVal(info[2], item.data))
            {
                continue;
            }
            results.push_back(std::move(item));
        }
    }

    return true;
}

static bool
getTotalUsedBytes(const std::string &lv_size, const std::string &data_percent, uint64_t &total_bytes, uint64_t &used_bytes, CondorError &err)
{
    long long total_size;
    try {
        total_size = std::stoll(lv_size);
    } catch (...) {
        err.pushf("VolumeManager", 20, "Failed to convert LV size to integer: %s", lv_size.c_str());
        return false;
    }
    // Note: data_percent always has the precision of 2 decimal points; hence
    // we convert this into an integer math problem.
    double data_percent_val;
    try {
        data_percent_val = std::stod(data_percent);
    } catch (...) {
        err.pushf("VolumeManager", 20, "Failed to convert LV percent used to float: %s", data_percent.c_str());
        return false;
    }
    total_bytes = total_size;
    uint64_t data_percent_100x_integer = data_percent_val * 100;
    // Special case: if all space is used, avoid potential rounding errors.
    if (data_percent == "100.00") {
        used_bytes = total_bytes;
    } else {
        used_bytes = total_size * data_percent_100x_integer / 10000;
    }
    return true;
}


bool
VolumeManager::GetPoolSize(uint64_t &used_bytes, uint64_t &total_bytes, CondorError &err)
{
    if (m_volume_group_name.empty() || (m_use_thin_provision && m_pool_lv_name.empty())) {return false;}
    return GetPoolSize(*this, used_bytes, total_bytes, err);
}


bool
VolumeManager::GetPoolSize(const VolumeManager& info, uint64_t &used_bytes, uint64_t &total_bytes, CondorError &err)
{
    bool thin = info.IsThin();
    std::vector<LVMReportItem> report;
    LVMReportFilter filter(info.GetVG());
    if (thin) {
        filter.thinpool = info.GetPool();
        filter.AddLV(info.GetPool());
    }

    if ( ! getLVMReport(report, err, filter, info.GetTimeout(), thin)) {
        return false;
    }

    if (report.size() != 1) {
        std::string debug_name = info.GetVG();
        if (thin) { debug_name += "/" + info.GetPool(); }
        err.pushf("VolumeManager", 18, "LVM Report for %s returned %zu items instead of just one",
                  debug_name.c_str(), report.size());
        return false;
    }

    LVMReportItem& provision = report[0];
    if (thin) {
        if ( ! getTotalUsedBytes(provision.size, provision.data, total_bytes, used_bytes, err)) {
            return false;
        }
    } else {
        uint64_t vg_bytes_free;
        try {
            vg_bytes_free = std::stoll(provision.data);
        } catch(...) {
            err.pushf("VolumeManager", 18, "Failed to convert VG free space to integer: %s",
                      provision.data.c_str());
            return false;
        }
        try {
            total_bytes = std::stoll(provision.size);
        } catch(...) {
            err.pushf("VolumeManager", 18, "Failed to convert VG total size to integer: %s",
                      provision.size.c_str());
            return false;
        }
        used_bytes = total_bytes - vg_bytes_free;
    }

    return true;
}


bool
VolumeManager::GetVolumeUsage(const VolumeManager::Handle* handle, filesize_t &used_bytes, size_t &numFiles, bool &out_of_space, CondorError &err)
{
    if ( ! handle || ! handle->HasInfo()) {
        err.pushf("VolumeManager", 19, "VolumeManager doesn't know its volume group or logical volume name.");
        return false;
    }

    std::string fs = handle->GetMount();
    struct statvfs fs_info;
    if (statvfs(fs.c_str(), &fs_info) != 0) {
        err.pushf("VolumeManager", 9, "Failed to get filesystem info for %s: Error(%d) %s\n",
                  fs.c_str(), errno, strerror(errno));
        return false;
    }

    // Get block size
    uint64_t bsize = fs_info.f_frsize ? fs_info.f_frsize : fs_info.f_bsize;
    // Calculate used bytes: (total blocks - blocks free) * block size
    uint64_t used = (fs_info.f_blocks - fs_info.f_bfree) * bsize;
    // If used bytes >= non-root space then set total block usage else set calculated usage
    used_bytes = static_cast<filesize_t>((used >= (used + fs_info.f_bavail * bsize)) ? (fs_info.f_blocks * bsize) : used);
    numFiles = static_cast<size_t>(fs_info.f_files - fs_info.f_ffree);
    dprintf(D_FULLDEBUG, "%s LV disk usage (statvfs): %lu bytes\n", fs.c_str(), used_bytes);

    // TODO: Startd not the Starter should find this info and take action
    if (handle->IsThin() && ! out_of_space) {
        const std::string& pool = handle->GetPool();
        std::vector<LVMReportItem> report;
        LVMReportFilter filter(handle->GetVG(), pool);
        filter.AddLV(pool);
        if ( ! getLVMReport(report, err, filter, handle->GetTimeout())) {
            return false;
        }
        for (const auto& lv : report) {
            if (lv.name != pool) { continue; }
            uint64_t reported_total_bytes, reported_used_bytes;
            if ( ! getTotalUsedBytes(lv.size, lv.data, reported_total_bytes, reported_used_bytes, err)) {
                return false;
            }
            if (reported_total_bytes == reported_used_bytes) {
                out_of_space = true;
                break;
            }
        }
    }
    return true;
}

bool
VolumeManager::CleanupAllDevices(const VolumeManager &info, CondorError &err, bool cleanup_loopback)
{
    bool had_failure = false;
    std::string pool_name = info.GetPool();
    std::string vg_name = info.GetVG();
    std::string loopdev = info.GetLoopDev();
    int timeout = info.GetTimeout();

    if ( ! pool_name.empty()) {
        int ret = RemoveLV(pool_name, vg_name, err, false, timeout);
        if (ret) {
            if (ret == 2) {
                dprintf(D_ALWAYS, "Warning: Backing thinpool '%s/%s' did not exist at removal time as expected!\n",
                        vg_name.c_str(), pool_name.c_str());
            } else { had_failure = true; }
        }
    }
    if ( ! vg_name.empty()) {
        had_failure |= !RemoveVG(vg_name, err, timeout);
    }
    if ( ! loopdev.empty()) {
        had_failure |= !RemovePV(loopdev, err, timeout);
        if (cleanup_loopback) {
            had_failure |= !RemoveLoopDev(loopdev, info.GetLoopFile(), err, timeout);
        }
    }

    return !had_failure;
}


bool
VolumeManager::CleanupLVs() {
    dprintf(D_FULLDEBUG, "Cleaning up all logical volumes associated with HTCondor.\n");

    CondorError err;
    std::vector<LVMReportItem> report;
    LVMReportFilter filter(m_volume_group_name, m_pool_lv_name);
    filter.SkipThinpool();

    if ( ! getLVMReport(report, err, filter, m_cmd_timeout)) {
        std::string debug_name = m_volume_group_name;
        if (m_use_thin_provision) { debug_name += "/" + m_pool_lv_name; }
        dprintf(D_ERROR, "Error: Failed to list logical volumes during cleanup of %s: %s\n",
                         debug_name.c_str(), err.getFullText().c_str());
        return false;
    }

    bool success = true;
    for (const auto& lv : report) {
        err.clear();

        if (lv.name == m_pool_lv_name) { // Extra protection to not remove thinpool LV here
            dprintf(D_ALWAYS, "Uh Oh! LVM Report did not filter out thinpool lv %s... skipping\n",
                    m_pool_lv_name.c_str());
            continue;
        }

        if (RemoveLV(lv.name, m_volume_group_name, err, lv.encrypted, m_cmd_timeout)) {
            dprintf(D_ALWAYS, "Failed to delete logical volume %s: %s\n",
                    lv.name.c_str(), err.getFullText().c_str());
            success = false;
        }
    }

    return success;
}


bool
VolumeManager::IsSetup() {
    bool lvm_setup = true;

    CondorError err;
    std::vector<LVMReportItem> report;
    LVMReportFilter filter(m_volume_group_name);

    if ( ! getLVMReport(report, err, filter, m_cmd_timeout, false)) {
        dprintf(D_ERROR, "Error: Failed to get volume group (%s) information: %s\n",
                         m_volume_group_name.c_str(), err.getFullText().c_str());
        return false;
    } else if (report.size() != 1) {
        dprintf(D_ERROR, "LVM Report for %s returned %zu items instead of just one.",
                m_volume_group_name.c_str(), report.size());
        return false;
    }

    if (report[0].name != m_volume_group_name) { lvm_setup = false; }

    if ( ! m_pool_lv_name.empty()) {
        struct stat statbuf;
        if (stat(DevicePath(m_volume_group_name, m_pool_lv_name).c_str(), &statbuf)) {
            lvm_setup = false;
        }
    }

    return lvm_setup;
}


void
VolumeManager::UpdateStarterEnv(Env &env, const std::string & lv_name, long long disk_kb, bool encrypt)
{
    if (m_volume_group_name.empty()) { return; }
    env.SetEnv("CONDOR_LVM_VG", m_volume_group_name);
    env.SetEnv("CONDOR_LVM_LV_NAME", lv_name);
    env.SetEnv("CONDOR_LVM_THIN_PROVISION", m_use_thin_provision ? "True" : "False");
    env.SetEnv("CONDOR_LVM_ENCRYPT", encrypt ? "True" : "False");
    if (disk_kb >= 0) { // treat negative values as undefined
        std::string size;
        formatstr(size, "%lld", disk_kb);
        env.SetEnv("CONDOR_LVM_LV_SIZE_KB", size.c_str());
    }

    if ( ! m_use_thin_provision || m_pool_lv_name.empty()) { return; }
    env.SetEnv("CONDOR_LVM_THINPOOL", m_pool_lv_name);
}


void
VolumeManager::AdvertiseInfo(ClassAd* ad){
    if ( ! ad) { return; }

    ad->Assign(ATTR_DISK_USAGE_ENFORCED, true);

    if ( ! m_loopdev_name.empty()) { ad->Assign(ATTR_LVM_USE_LOOPBACK, true); }
    if (m_use_thin_provision) { ad->Assign(ATTR_LVM_USE_THIN_PROVISION, true); }
}


#else
   // dummy volume manager for ! LINUX

VolumeManager::VolumeManager() {
}

VolumeManager::~VolumeManager() {
}

void VolumeManager::AdvertiseInfo(ClassAd* /*ad*/){
}

void VolumeManager::UpdateStarterEnv(Env& /*env*/, const std::string& /*lv_name*/, long long /*disk_kb*/, bool /*encrypt*/) {
}

int VolumeManager::CleanupLV(const std::string& /*lv_name*/, CondorError& /*err*/, int /*is_encrypted*/) {
    return 0;
}

bool VolumeManager::CleanupLVs() {
    return true;
}

bool VolumeManager::GetPoolSize(uint64_t& /*used_bytes*/, uint64_t& /*total_bytes*/, CondorError& /*err*/) {
    return false;
}

#endif
