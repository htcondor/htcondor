
#include <condor_common.h>
#include <CondorError.h>
#include <condor_config.h>
#include <condor_debug.h>
#include <condor_crypt.h>
#include <my_popen.h>
#include <condor_uid.h>
#include <subsystem_info.h>

#include <rapidjson/document.h>

#include <sys/mount.h>

#include "VolumeManager.h"

#include <sstream>

#define VOLUME_MANAGER_TIMEOUT 120


VolumeManager::VolumeManager()
    : m_encrypt(param_boolean("STARTD_ENCRYPT_EXECUTE_DISK", false))
{
    if (!param_boolean("STARTD_ENFORCE_DISK_LIMITS", false)) {
        dprintf(D_FULLDEBUG, "Not enforcing disk limits in the startd.\n");
        return;
    }
    std::string pool_name; std::string volume_group_name;
    if (!param(pool_name, "THINPOOL_NAME") || !param(volume_group_name, "THINPOOL_VOLUME_GROUP_NAME")) {
        param(m_loopback_filename, "THINPOOL_BACKING_FILE", "$(SPOOL)/startd_disk.img");
        uint64_t size_mb = param_integer("THINPOOL_BACKING_SIZE_MB", 10240);
        CondorError err;
        dprintf(D_ALWAYS, "StartD will enforce disk limits using a loopback file %s.\n", m_loopback_filename.c_str());
        m_loopdev_name = CreateLoopback(m_loopback_filename, size_mb*1024, err);
        if (m_loopdev_name.empty()) {
            dprintf(D_ALWAYS, "Failed to allocate thinpool loopback device: %s\n", err.getFullText().c_str());
            return;
        }

        auto lname = get_mySubSystem()->getLocalName();
        std::string condor_prefix = "_condor_" + (lname ? (std::string(lname) + "_") : "") + get_mySubSystem()->getName();
            // Preemptively clean-up condor-owned devices.
        CleanupAllDevices("", m_loopdev_name, condor_prefix, condor_prefix, err);
        err.clear();

        if (!CreatePV(m_loopdev_name, err)) {
            dprintf(D_ALWAYS, "Failed to create LVM physical volume for loopback device: %s\n", err.getFullText().c_str());
            return;
        }

        if (!CreateVG(condor_prefix, m_loopdev_name, err)) {
            dprintf(D_ALWAYS, "Failed to create LVM volume group for loopback device %s: %s\n",
                m_loopdev_name.c_str(), err.getFullText().c_str());
            return;
        }
        m_vg_name = condor_prefix;

        if (!CreateThinPool(condor_prefix, condor_prefix, err)) {
            dprintf(D_ALWAYS, "Failed to create LVM thin logical volume for loopback device %s: %s\n",
                m_loopdev_name.c_str(), err.getFullText().c_str());
            return;
        }
        m_volume_name = m_pool_name = condor_prefix;
        m_volume_group_name = m_vg_name;
    } else {
        dprintf(D_ALWAYS, "StartD will enforce disk limits using a logical volume named %s.\n", pool_name.c_str());
        m_volume_name = pool_name;
        m_volume_group_name = volume_group_name;
    }
}

VolumeManager::~VolumeManager()
{
    if (!m_pool_name.empty()) {
        CondorError err;
        auto lvs = ListPoolLVs(m_pool_name, err);
        if (!err.empty()) {
            dprintf(D_ALWAYS, "Failed to list logical volumes when cleaning up volume manager: %s\n",
                err.getFullText().c_str());
        }
        for (const auto & lv : lvs) {
            err.clear();
            if (!RemoveLV(lv, m_volume_group_name, err)) {
                dprintf(D_ALWAYS, "Failed to delete logical volume %s: %s\n",
                    lv.c_str(), err.getFullText().c_str());
            }
        }
    }
    if (!m_loopback_filename.empty()) {
        CondorError err;
        if (!CleanupAllDevices(m_loopdev_name, m_loopdev_name, m_vg_name, m_pool_name, err)) {
            dprintf(D_ALWAYS, "Failed to cleanup volume manager devices: %s\n", err.getFullText().c_str());
        }
    }
}


VolumeManager::Handle::Handle(const std::string &mountpoint, const std::string &volume, const std::string &pool, const std::string &vg_name, uint64_t size_kb, CondorError &err)
{
    auto extra_volume_size_mb = param_integer("THINPOOL_EXTRA_SIZE_MB", 0);
    size_kb += 1024*extra_volume_size_mb;

    if (!VolumeManager::CreateThinLV(size_kb, volume, pool, vg_name, err)) {
        return;
    }
    m_volume = volume;
    m_vg_name = vg_name;
    std::string device_path = "/dev/mapper/" + vg_name + "-" + volume;
    if (!VolumeManager::CreateFilesystem(volume, device_path, err)) {
        return;
    }
    if (!VolumeManager::MountFilesystem(device_path, mountpoint, err)) {
        return;
    }
    m_mountpoint = mountpoint;
}


VolumeManager::Handle::~Handle()
{
    CondorError err;
    if (!m_mountpoint.empty()) {
        UnmountFilesystem(m_mountpoint, err);
    }
    if (!m_volume.empty()) {
        RemoveLV(m_volume, m_vg_name, err);
    }
    if (!err.empty()) {
        dprintf(D_ALWAYS, "Errors when cleaning up starter mounts: %s\n", err.getFullText().c_str());
    }
}


bool
VolumeManager::CleanupSlot(const std::string &slot, CondorError &err)
{
    dprintf(D_FULLDEBUG, "StartD is cleaning up logical volume for slot %s.\n", slot.c_str());
    if (!slot.empty() && !m_volume_group_name.empty()) {
        return RemoveLV(slot, m_volume_group_name, err);
    }
    return true;
}


bool
VolumeManager::MountFilesystem(const std::string &device_path, const std::string &mountpoint, CondorError &err)
{
    TemporaryPrivSentry sentry(PRIV_ROOT);

    if (-1 == unshare(CLONE_NEWNS)) {
        err.pushf("VolumeManager", 14, "Failed to create new mount namespace for starter: %s (errno=%d)",
            strerror(errno), errno);
        return false;
    }
    if (-1 == mount("", "/", "dontcare", MS_REC|MS_SLAVE, "")) {
        err.pushf("VolumeManager", 15, "Failed to unshare the mount namespace: %s (errno=%d)",
            strerror(errno), errno);
        return false;
    }

    // mount -o ,barrier=0 /dev/test_vg/condor_slot_1 /mnt/condor_slot_1
    ArgList args;
    args.AppendArg("mount");
    args.AppendArg("--no-mtab");
    args.AppendArg("-o");
    args.AppendArg("barrier=0,noatime,discard,nodev,nosuid,");
    args.AppendArg(device_path);
    args.AppendArg(mountpoint);
    int exit_status;
    std::unique_ptr<char> mount_output(
        run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                    args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 13, "Failed to mount new filesystem %s for job (exit status %d): %s\n", mountpoint.c_str(), exit_status, mount_output ? mount_output.get(): "(no command output)");
        return false;
    }

    if (-1 == chown(mountpoint.c_str(), get_real_condor_uid(), get_real_condor_gid())) {
        err.pushf("VolumeManager", 14, "Failed to chown the new execute mount to condor user: %s (errno=%d)",
            strerror(errno), errno);
        return false;
    }

    return true;
}


bool
VolumeManager::CreateFilesystem(const std::string &label, const std::string &devname, CondorError &err)
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
    int exit_status;
    std::unique_ptr<char> mke2fs_output(
        run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                    args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 13, "Failed to create new ext4 filesystem for job (exit status %d): %s\n", exit_status, mke2fs_output ? mke2fs_output.get(): "(no command output)");
        return false;
    }
    return true;
}


std::string
VolumeManager::CreateLoopback(const std::string &filename, uint64_t size_kb, CondorError &err)
{
    if (filename.empty()) {
        err.push("VolumeManager", 1, "Loopback filenamae is empty");
        return "";
    }
    if (!size_kb) {
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
        int exit_status;
        std::unique_ptr<char> losetup_output(
            run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
        if (exit_status) {
            err.pushf("VolumeManager", 3, "Failed to list current loopback devices (exit status %d): %s\n", exit_status, losetup_output ? losetup_output.get(): "(no command output)");
            return "";
        }

        std::stringstream ss(losetup_output.get());
        bool found_header = false;
        for (std::string line; std::getline(ss, line); ) {
            if (!found_header) {
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
        fstat(fd, &backing_stat);
        close(fd);
        if (alloc_error) {
            err.pushf("VolumeManager", 5, "Failed to allocate space for data in %s: %s (errno=%d)",
                filename.c_str(), strerror(alloc_error), alloc_error);
            return "";
        }
    }


        // TODO: Would be friendlier to unmount everything and resize instead of failing
    if (backing_stat.st_size < static_cast<off_t>(size_kb*1.02)*1024) {
        err.push("VolumeManager", 6, "Pre-existing loopback file is too small");
        return "";
    }

    if (!existing_device) {
        dprintf(D_FULLDEBUG, "Loopback device does not already exist; creating for file %s.\n", filename.c_str());
        ArgList args;
        args.AppendArg("losetup");
        args.AppendArg("--find");
        args.AppendArg("--show");
        args.AppendArg(filename.c_str());
        int exit_status;
        std::unique_ptr<char> losetup_output(
            run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
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
VolumeManager::CreatePV(const std::string &devname, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Creating LVM metadata for physical volume %s.\n", devname.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("pvcreate");
    args.AppendArg(devname);
    int exit_status;
    std::unique_ptr<char> pvcreate_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM metadata for physical volume %s (exit status %d): %s",
            devname.c_str(), exit_status, pvcreate_output ? pvcreate_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::CreateVG(const std::string &vg_name, const std::string &devname, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Creating LVM volume group %s for physical volume %s.\n", vg_name.c_str(), devname.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("vgcreate");
    args.AppendArg(vg_name);
    args.AppendArg(devname);
    int exit_status;
    std::unique_ptr<char> vgcreate_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM volume group for physical volume %s (exit status %d): %s",
            devname.c_str(), exit_status, vgcreate_output ? vgcreate_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::CreateThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Creating LVM thin logical volume %s for volume group %s.\n", lv_name.c_str(), vg_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("lvcreate");
    args.AppendArg("--type");
    args.AppendArg("thin-pool");
    args.AppendArg("-l");
    args.AppendArg("100\%FREE");
    args.AppendArg(vg_name);
    args.AppendArg("-n");
    args.AppendArg(lv_name);
    int exit_status;
    std::unique_ptr<char> lvcreate_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM thin logical volume pool %s for volume group %s (exit status %d): %s",
            lv_name.c_str(), vg_name.c_str(), exit_status, lvcreate_output ? lvcreate_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::CreateThinLV(uint64_t size_kb, const std::string &lv_name_input, const std::string &pool_name, const std::string &vg_name, CondorError &err)
{
    std::string lv_name = lv_name_input;
    bool do_encrypt = lv_name_input.substr(lv_name_input.size() - 4, 4) == "-enc";
    if (do_encrypt) {
        lv_name = lv_name.substr(0, lv_name.size() - 4);
    }

    dprintf(D_FULLDEBUG, "Creating LVM thin logical volume %s for volume group %s.\n", lv_name.c_str(), vg_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    // lvcreate -V 1G -T test_vg/condor_thinpool -n condor_slot_1
    args.AppendArg("lvcreate");
    args.AppendArg("-V");
    std::string size;
    formatstr(size, "%luk", size_kb);
    args.AppendArg(size);
    args.AppendArg("-T");
    args.AppendArg(vg_name + "/" + pool_name);
    args.AppendArg("-n");
    args.AppendArg(lv_name);
    int exit_status;
    std::unique_ptr<char> lvcreate_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to create LVM thin logical volume %s for volume group %s (exit status %d): %s",
            lv_name.c_str(), vg_name.c_str(), exit_status, lvcreate_output ? lvcreate_output.get() : "(no output)");
        return false;
    }

    if (do_encrypt) return EncryptThinPool(lv_name, vg_name, err);
    return true;
}


// 256-bit encryption
#define KEY_SIZE 32
bool
VolumeManager::EncryptThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err)
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

    std::unique_ptr<unsigned char> key(Condor_Crypt_Base::randomKey(KEY_SIZE));
    if (!key) {
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
    args.AppendArg("/dev/mapper/" + vg_name + "-" + lv_name);
    args.AppendArg(vg_name + "-" + lv_name + "-enc");
    int exit_status;
    std::unique_ptr<char> cryptsetup_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
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

    std::unique_ptr<char> cwd(get_current_dir_name());
    if (-1 == chdir("/")) {
        err.pushf("VolumeManager", 16, "Failed to change directory: %s (errno=%d)",
            strerror(errno), errno);
    }
    int exit_status = 0;
    if (-1 == umount(mountpoint.c_str())) {
        exit_status = errno;
        err.pushf("VolumeManager", 17, "Failed to unmount %s: %s (errno=%d)",
            mountpoint.c_str(), strerror(errno), errno);
    }
    if (cwd) {
        if (-1 == chdir(cwd.get())) {
            err.pushf("VolumeManager", 16, "Failed to change directory: %s (errno=%d)",
                 strerror(errno), errno);
        }
    }
    if (exit_status) {
        return false;
    }
    return true;
}


bool
VolumeManager::RemoveEncryptedThinPool(const std::string &lv_name, const std::string &vg_name, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Removing encrypted volume %s.\n", lv_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("cryptsetup");
    args.AppendArg("close");
    args.AppendArg(vg_name + "-" + lv_name);
    int exit_status;
    std::unique_ptr<char> cryptsetup_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete encrypted volume %s (exit status %d): %s",
            lv_name.c_str(), exit_status, cryptsetup_output ? cryptsetup_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::RemoveLV(const std::string &lv_name_input, const std::string &vg_name, CondorError &err)
{
    TemporaryPrivSentry sentry(PRIV_ROOT);

    std::string lv_name = lv_name_input;
        // We know we are removing an encrypted logical volume; first invoke
        // 'cryptsetup close'
    if (lv_name.substr(lv_name.size() - 4, 4) == "-enc") {
        RemoveEncryptedThinPool(lv_name, vg_name, err);
        lv_name = lv_name.substr(0, lv_name.size() - 4);
    } else {
        // In some cases, we are iterating through all known LVM LVs (which doesn't include
        // the encrypted volumes); if so, we check first if an encrypted volume exists..
        std::string encrypted_name = lv_name + "-enc";
        struct stat statbuf;
        if (-1 != stat(("/dev/mapper/" + vg_name + "-" + encrypted_name).c_str(), &statbuf)) {
            RemoveEncryptedThinPool(encrypted_name, vg_name, err);
        }
    }

    dprintf(D_FULLDEBUG, "Removing logical volume %s.\n", lv_name_input.c_str());

    ArgList args;
    args.AppendArg("lvremove");
    args.AppendArg(vg_name + "/" + lv_name);
    args.AppendArg("--yes");
    int exit_status;
    std::unique_ptr<char> lvremove_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete logical volume %s (exit status %d): %s",
            lv_name.c_str(), exit_status, lvremove_output ? lvremove_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::RemoveVG(const std::string &vg_name, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Removing LVM volume group %s.\n", vg_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("vgremove");
    args.AppendArg(vg_name);
    args.AppendArg("--yes");
    int exit_status;
    std::unique_ptr<char> vgremove_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete volume group %s (exit status %d): %s",
            vg_name.c_str(), exit_status, vgremove_output ? vgremove_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::RemovePV(const std::string &pv_name, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Removing physical volume %s.\n", pv_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("pvremove");
    args.AppendArg(pv_name);
    args.AppendArg("--yes");
    int exit_status;
    std::unique_ptr<char> pvremove_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete LVM physical volume %s (exit status %d): %s",
            pv_name.c_str(), exit_status, pvremove_output ? pvremove_output.get() : "(no output)");
        return false;
    }
    return true;
}


bool
VolumeManager::RemoveLoopDev(const std::string &loopdev_name, CondorError &err)
{
    dprintf(D_FULLDEBUG, "Removing loop devices %s.\n", loopdev_name.c_str());
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("losetup");
    args.AppendArg("-d");
    args.AppendArg(loopdev_name);
    int exit_status;
    std::unique_ptr<char> losetup_output(
                run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                        args, RUN_COMMAND_OPT_WANT_STDERR, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 12, "Failed to delete loopback device %s (exit status %d): %s",
            loopdev_name.c_str(), exit_status, losetup_output ? losetup_output.get() : "(no output)");
        return false;
    }
    return true;
}


namespace {

bool
getLVSReport(CondorError &err, rapidjson::Value &result)
{
    TemporaryPrivSentry sentry(PRIV_ROOT);
    ArgList args;
    args.AppendArg("lvs");
    args.AppendArg("--reportformat");
    args.AppendArg("json");
    args.AppendArg("--units");
    args.AppendArg("b");
    int exit_status;
    std::unique_ptr<char> losetup_output(
        run_command(param_integer("VOLUME_MANAGER_TIMEOUT", VOLUME_MANAGER_TIMEOUT),
                    args, 0, nullptr, &exit_status));
    if (exit_status) {
        err.pushf("VolumeManager", 9, "Failed to list logical volumes (exit status=%d): %s",
            exit_status, losetup_output ? losetup_output.get() : "(no output)");
        return false;
    }

    rapidjson::Document doc;
    if (doc.Parse(losetup_output ? losetup_output.get() : "").HasParseError()) {
        err.pushf("VolumeManager", 10, "Failed to parse logical volume status as JSON: %s",
            losetup_output ? losetup_output.get() : "(no output)");
        return false;
    }
    if (!doc.IsObject() || !doc.HasMember("report")) {
        err.pushf("VolumeManager", 11, "Invalid JSON from logical volume status: %s", losetup_output.get());
        return false;
    }
    auto &report_obj = doc["report"];
    if (!report_obj.IsArray() || !report_obj.Size()) {
        err.pushf("VolumeManager", 11, "Invalid JSON from logical volume status (no LV report): %s", losetup_output.get());
        return false;
    }
    for (auto iter = report_obj.Begin(); iter != report_obj.End(); ++iter) {
         const auto &report_entry = *iter;
         if (!report_entry.IsObject() || !report_entry.HasMember("lv") || !report_entry["lv"].IsArray()) {continue;}
         result.CopyFrom(report_entry["lv"], doc.GetAllocator());
         return true;
    }
    err.pushf("VolumeManager", 11, "JSON result from logical volume status did not contain any logical volumes.\n");
    return false;
}


bool
getTotalUsedBytes(const std::string &lv_size, const std::string &data_percent, uint64_t &total_bytes, uint64_t &used_bytes, CondorError &err)
{
	long long total_size;
	try {
		total_size = std::stoll(lv_size);
	} catch (...) {
		err.pushf("VolumeManager", 20, "Failed to convert size to integer: %s", lv_size.c_str());
		return false;
	}
		// Note: data_percent always has the precision of 2 decimal points; hence
		// we convert this into an integer math problem.
	double data_percent_val;
	try {
		data_percent_val = std::stod(data_percent);
	} catch (...) {
		err.pushf("VolumeManager", 20, "Failed to convert percent used to float: %s", data_percent.c_str());
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

}


bool
VolumeManager::GetPoolSize(uint64_t &used_bytes, uint64_t &total_bytes, CondorError &err)
{
    if (m_volume_name.empty() || m_volume_group_name.empty()) {return false;}
    return GetPoolSize(m_volume_name, m_volume_group_name, used_bytes, total_bytes, err);
}


bool
VolumeManager::GetPoolSize(const std::string &pool_name, const std::string &vg_name, uint64_t &used_bytes, uint64_t &total_bytes, CondorError &err)
{
    rapidjson::Value lvs_report;
    std::vector<std::string> lvs;
    if (!getLVSReport(err, lvs_report)) {
        return false;
    }
    for (auto iter = lvs_report.Begin(); iter != lvs_report.End(); ++iter) {
         const auto &lv_report = *iter;
         if (!lv_report.IsObject() || !lv_report.HasMember("lv_name") || !lv_report["lv_name"].IsString() ||
             !lv_report.HasMember("lv_size") || !lv_report["lv_size"].IsString() || !lv_report.HasMember("vg_name") ||
             !lv_report["vg_name"].IsString() || lv_report["vg_name"].GetString() != vg_name ||
             lv_report["lv_name"].GetString() != pool_name || !lv_report.HasMember("data_percent") || !lv_report["data_percent"].IsString())
         {
             continue;
         }
         if (!getTotalUsedBytes(lv_report["lv_size"].GetString(), lv_report["data_percent"].GetString(), total_bytes, used_bytes, err)) {
             return false;
         }
         return true;
    }
    err.pushf("VolumeManager", 18, "LVS failed to provide information about %s in its output.", pool_name.c_str());
    return false;
}


bool
VolumeManager::GetThinVolumeUsage(const std::string &thin_volume_name, const std::string &volume_name, const std::string &volume_group_name, uint64_t &used_bytes, bool &out_of_space, CondorError &err)
{
	rapidjson::Value lvs_report;
	std::vector<std::string> lvs;
	if (!getLVSReport(err, lvs_report)) {
		return false;
	}
	if (volume_name.empty() || volume_group_name.empty()) {
		err.pushf("VolumeManager", 19, "VolumeManager doesn't know its VG or volume name.");
		return false;
	}
	bool found_thin_volume = false;
	for (auto iter = lvs_report.Begin(); iter != lvs_report.End(); ++iter) {
		const auto &lv_report = *iter;
		if (!lv_report.IsObject() || !lv_report.HasMember("lv_name") || !lv_report["lv_name"].IsString() ||
		    !lv_report.HasMember("lv_size") || !lv_report["lv_size"].IsString() || !lv_report.HasMember("vg_name") ||
		    !lv_report["vg_name"].IsString() || lv_report["vg_name"].GetString() != volume_group_name ||
		    !lv_report.HasMember("pool_lv") || !lv_report["pool_lv"].IsString() ||
		    !lv_report.HasMember("data_percent") || !lv_report["data_percent"].IsString())
		{
			continue;
		}
		uint64_t reported_total_bytes, reported_used_bytes;
		if (!getTotalUsedBytes(lv_report["lv_size"].GetString(), lv_report["data_percent"].GetString(), reported_total_bytes, reported_used_bytes, err)) {
			return false;
		}
		if ((lv_report["lv_name"].GetString() == volume_name) && (reported_total_bytes == reported_used_bytes)) {
			out_of_space = true;
		}
		if ((lv_report["lv_name"].GetString() == thin_volume_name) && (lv_report["pool_lv"].GetString() == volume_name)) {
			found_thin_volume = true;
			used_bytes = reported_used_bytes;
		}
	}
	return found_thin_volume;
}


std::vector<std::string>
VolumeManager::ListPoolLVs(const std::string &pool_name, CondorError &err)
{
    rapidjson::Value lvs_report;
    std::vector<std::string> lvs;
    if (!getLVSReport(err, lvs_report)) {
        return lvs;
    }
    for (auto iter = lvs_report.Begin(); iter != lvs_report.End(); ++iter) {
         const auto &lv_report = *iter;
         if (!lv_report.IsObject() || !lv_report.HasMember("pool_lv") || !lv_report.HasMember("lv_name") ||
             !lv_report["pool_lv"].IsString() || !lv_report["lv_name"].IsString())
         {
             continue;
         }
         if (lv_report["pool_lv"].GetString() == pool_name) {
             lvs.emplace_back(lv_report["lv_name"].GetString());
         }
    }
    return lvs;
}

bool
VolumeManager::CleanupAllDevices(const std::string &loopdev, const std::string &pv_name, const std::string &vg_name, const std::string &pool_name, CondorError &err)
{
    bool had_failure = false;
    if (!pool_name.empty()) {
        had_failure |= !RemoveLV(pool_name, vg_name, err);
    }
    if (!vg_name.empty()) {
        had_failure |= !RemoveVG(vg_name, err);
    }
    if (!pv_name.empty()) {
        had_failure |= !RemovePV(pv_name, err);
    }
    if (!loopdev.empty()) {
        had_failure |= !RemoveLoopDev(loopdev, err);
    }
    return !had_failure;
}


void
VolumeManager::UpdateStarterEnv(Env &env)
{
    if (m_volume_name.empty() || m_volume_group_name.empty()) {return;}
    env.SetEnv("_CONDOR_THINPOOL", m_volume_name + (m_encrypt ? "-enc" : ""));
    env.SetEnv("_CONDOR_THINPOOL_VG", m_volume_group_name);
}
