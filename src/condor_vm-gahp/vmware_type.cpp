/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "basename.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "util_lib_proto.h"
#include "stat_wrapper.h"
#include "vmgahp_common.h"
#include "vmgahp_error_codes.h"
#include "condor_vm_universe_types.h"
#include "vmware_type.h"

#define VMWARE_TMP_FILE "vmware_status.condor"
#define VMWARE_TMP_TEMPLATE		"vmXXXXXX"
#define VMWARE_TMP_CONFIG_SUFFIX	"_condor.vmx"
#define VMWARE_VMX_FILE_PERM	0770
#define VMWARE_VMDK_FILE_PERM	0660

#define VMWARE_LOCAL_SETTINGS_PARAM "VMWARE_LOCAL_SETTINGS_FILE"
#define VMWARE_LOCAL_SETTINGS_START_MARKER "### Start local parameters ###"
#define VMWARE_LOCAL_SETTINGS_END_MARKER "### End local parameters ###"

#define VMWARE_MONOLITHICSPARSE_VMDK_SEEK_BYTE	512
#define VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE	800

#define VMWARE_SNAPSHOT_PARENTFILE_HINT "parentFileNameHint"

extern uid_t job_user_uid;
extern std::string workingdir;

// "parent_filenames" will have basenames for parent files
static void
change_monolithicSparse_snapshot_vmdk_file(const char* file, bool use_fullpath, const char* dirpath, StringList &parent_filenames)
{
	if( !file || (check_vm_read_access_file(file) == false) 
			|| ( use_fullpath && !dirpath )) {
		return;
	}

	// read snapshot vmdk file to find "parentFileNameHint"
	int fd = -1;
	fd = safe_open_wrapper_follow(file, O_RDWR);
	if( fd < 0 ) {
		vmprintf(D_ALWAYS, "failed to safe_open_wrapper file(%s) : "
				"safe_open_wrapper returns %s\n", file, strerror(errno));
		return;
	}

	int ret = lseek(fd, VMWARE_MONOLITHICSPARSE_VMDK_SEEK_BYTE, SEEK_SET);
	if( ret != VMWARE_MONOLITHICSPARSE_VMDK_SEEK_BYTE ) {
		close(fd);
		vmprintf(D_ALWAYS, "failed to lseek to %d in file(%s). "
				"Is this file a vmdk file for vmware monolithicsparse disk?\n", 
				VMWARE_MONOLITHICSPARSE_VMDK_SEEK_BYTE, file);
		return;
	}

	char descbuffer[VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE + 1];

	ret = read(fd, descbuffer, VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE );
	if( ret != VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE ) {
		close(fd);
		vmprintf(D_ALWAYS, "failed to read(need %d but real read %d) in file(%s). "
				"Is this file a vmdk file for vmware monolithicsparse disk?\n", 
				VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE, ret, file);
		return;
	}

	descbuffer[VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE] = '\0';

	char* namestartpos = strstr(descbuffer, VMWARE_SNAPSHOT_PARENTFILE_HINT);
	if( !namestartpos ) {
		close(fd);
		vmprintf(D_ALWAYS, "failed to find(%s) in file(%s). "
				"Is this file a vmdk file for vmware monolithicsparse disk?\n", 
				VMWARE_SNAPSHOT_PARENTFILE_HINT, file);
		return;
	}

	namestartpos += strlen(VMWARE_SNAPSHOT_PARENTFILE_HINT);
	while( *namestartpos == ' ' || *namestartpos == '=' || *namestartpos == '\"' ) {
		namestartpos++;
	}

	char* tmppos = namestartpos;
	std::string parentfilename;

	while( *tmppos != '\"' ) {
		parentfilename += *tmppos++;
	}

	char* nameendpos = tmppos; 

	vmprintf(D_FULLDEBUG, "parentfilename is %s in file(%s)\n",
			parentfilename.c_str(), file);

	parent_filenames.append(condor_basename(parentfilename.c_str()));

	std::string final_parentfilename;
	bool is_modified = false;

	if( use_fullpath ) {
		// We need fullpath 
		if( fullpath(parentfilename.c_str()) == false ) {
			// parentfilename is not fullpath
			if(	dirpath[0] == '/' ) {
				// submitted from Linux machine
				formatstr(final_parentfilename, "%s/%s", dirpath, parentfilename.c_str());
			}else {
				// submitted from Windows machine
				formatstr(final_parentfilename, "%s\\%s", dirpath, parentfilename.c_str());
			}
			is_modified = true;
		}
	}else {
		// We need basename
		if( fullpath(parentfilename.c_str()) ) {
			// parentfilename is fullpath
			final_parentfilename = condor_basename(parentfilename.c_str());
			is_modified = true;
		}
	}

	if( !is_modified ) {
		close(fd);
		return;
	}

	// Modifying..
	int index = nameendpos - descbuffer;
	if( final_parentfilename.length() > parentfilename.length() ) {
		// we converted basename to fullpath
		memmove(namestartpos + final_parentfilename.length(), nameendpos, 
				VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE - index - final_parentfilename.length()); 
		memcpy(namestartpos, final_parentfilename.c_str(), final_parentfilename.length());
	}else {
		// we converted fullpath to basename
		memmove(namestartpos + final_parentfilename.length(), nameendpos, 
				VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE - index); 
		memcpy(namestartpos, final_parentfilename.c_str(), final_parentfilename.length());

		int remain = parentfilename.length() - final_parentfilename.length();
		if( remain > 0 ) {
			memset(descbuffer + VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE - remain, 
					0, remain);
		}
	}

	ret = lseek(fd, VMWARE_MONOLITHICSPARSE_VMDK_SEEK_BYTE, SEEK_SET);
	if( ret != VMWARE_MONOLITHICSPARSE_VMDK_SEEK_BYTE ) {
		close(fd);
		vmprintf(D_ALWAYS, "failed to lseek to %d in file(%s) for modification\n",
				VMWARE_MONOLITHICSPARSE_VMDK_SEEK_BYTE, file);
		return;
	}

	ret = write(fd, descbuffer, VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE );
	if( ret != VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE ) {
		close(fd);
		vmprintf(D_ALWAYS, "failed to write %d in file(%s) for modification\n",
				VMWARE_MONOLITHICSPARSE_VMDK_DESCRIPTOR_SIZE, file);
		return;
	}

	close(fd);
}

// "parent_filenames" will have basenames for parent files
static void
change_snapshot_vmdk_file(const char* file, bool use_fullpath, const char* dirpath, StringList &parent_filenames)
{
	if( !file || (check_vm_read_access_file(file) == false) 
			|| ( use_fullpath && !dirpath )) {
		return;
	}

	// find the filesize of vmdk file
	int file_size = 0; 
	StatWrapper swrap(file);
	file_size = swrap.GetBuf()->st_size;

	// read snapshot vmdk file to find "parentFileNameHint"
	FILE *fp = NULL;
	fp = safe_fopen_wrapper_follow(file, "r");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper file(%s) : "
				"safe_fopen_wrapper returns %s\n", file, strerror(errno));
		return;
	}

	char linebuf[2048];
	StringList filelines;
	std::string tmp_line;
	std::string one_line;
	std::string name;
	std::string value;
	bool is_modified = false;
	int total_read = 0;

	while( fgets(linebuf, 2048, fp) ) {
		total_read += strlen(linebuf);
		one_line = linebuf;
		trim(one_line);

		if( one_line.length() == 0 ) {
			filelines.append(linebuf);
			continue;
		}

		if( one_line[0] == '#' ) {
			/* Skip over comments */
			filelines.append(linebuf);
			continue;
		}

		parse_param_string(one_line.c_str(), name, value, true);

		if( name.length() == 0 ) {
			filelines.append(linebuf);
			continue;
		}

		if( !strcasecmp(name.c_str(), VMWARE_SNAPSHOT_PARENTFILE_HINT) ) {

			parent_filenames.append(condor_basename(value.c_str()));

			if( use_fullpath ) {
				if( fullpath(value.c_str()) == false ) {
					std::string tmp_fullname;
			
					if(	dirpath[0] == '/' ) {
						// submitted from Linux machine
						formatstr(tmp_fullname, "%s/%s", dirpath, value.c_str());
					}else {
						// submitted from Windows machine
						formatstr(tmp_fullname, "%s\\%s", dirpath, value.c_str());
					}

					formatstr(tmp_line, "%s=\"%s\"\n", name.c_str(), tmp_fullname.c_str());
					filelines.append(tmp_line.c_str());
					is_modified = true;
					continue;
				}
			}else {
				if( fullpath(value.c_str()) ) {
					formatstr(tmp_line, "%s=\"%s\"\n", name.c_str(), condor_basename(value.c_str()));
					filelines.append(tmp_line.c_str());
					is_modified = true;
					continue;
				}
			}
		}
		filelines.append(linebuf);
	}
	fclose(fp);

	if( total_read != file_size ) {
		// Maybe, this is a vmdk file for monolithicSparse disk
		change_monolithicSparse_snapshot_vmdk_file(file, use_fullpath, dirpath, parent_filenames);
		return;
	}

	if( !is_modified ) {
		return;
	}

	// Overwriting..
	fp = safe_fopen_wrapper_follow(file, "w");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper file(%s) for "
				"overwriting : safe_fopen_wrapper returns %s\n", 
				file, strerror(errno));
		return;
	}

	char *line = NULL;
	filelines.rewind();
	while( (line = filelines.next()) != NULL ) {
		fprintf(fp,"%s", line);
	}
	fclose(fp);
}

// Input "parent_filenames" must have basenames for parent files
static void change_snapshot_vmsd_file(const char *file, StringList *parent_filenames, bool use_fullpath, const char* dirpath)
{
	if( !file || (check_vm_read_access_file(file) == false) 
			|| !parent_filenames || ( use_fullpath && !dirpath )) {
		return;
	}

	FILE *fp = NULL;
	fp = safe_fopen_wrapper_follow(file, "r");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper file(%s) : "
				"safe_fopen_wrapper returns %s\n", file, strerror(errno));
		return;
	}

	char linebuf[2048];
	StringList filelines;
	std::string tmp_line;
	std::string one_line;
	std::string name;
	std::string value;
	bool is_modified = false;

	while( fgets(linebuf, 2048, fp) ) {
		one_line = linebuf;
		trim(one_line);

		if( one_line.length() == 0 ) {
			filelines.append(linebuf);
			continue;
		}

		if( one_line[0] == '#' ) {
			/* Skip over comments */
			filelines.append(linebuf);
			continue;
		}

		parse_param_string(one_line.c_str(), name, value, true);

		if( name.length() == 0 ) {
			filelines.append(linebuf);
			continue;
		}

		std::string tmp_name = name;
		lower_case(tmp_name);
		if( tmp_name.find( ".filename", 0 ) != std::string::npos ) {
			if( parent_filenames->contains(condor_basename(value.c_str()))) {
				if( use_fullpath ) {
					if( fullpath(value.c_str()) == false ) {
						std::string tmp_fullname;

						if(	dirpath[0] == '/' ) {
							// submitted from Linux machine
							formatstr(tmp_fullname, "%s/%s", dirpath, value.c_str());
						}else {
							// submitted from Windows machine
							formatstr(tmp_fullname, "%s\\%s", dirpath, value.c_str());
						}

						formatstr(tmp_line, "%s = \"%s\"\n", name.c_str(), 
								tmp_fullname.c_str());
						filelines.append(tmp_line.c_str());
						is_modified = true;
						continue;
					}
				}else {
					if( fullpath(value.c_str()) ) {
						formatstr(tmp_line, "%s = \"%s\"\n", name.c_str(), 
								condor_basename(value.c_str()));
						filelines.append(tmp_line.c_str());
						is_modified = true;
						continue;
					}
				}
			}
		}
		filelines.append(linebuf);
	}
	fclose(fp);

	if( !is_modified ) {
		return;
	}

	// Overwriting..
	fp = safe_fopen_wrapper_follow(file, "w");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper file(%s) for overwriting : "
				"safe_fopen_wrapper returns %s\n", file, strerror(errno));
		return;
	}

	char *line = NULL;
	filelines.rewind();
	while( (line = filelines.next()) != NULL ) {
		fprintf(fp,"%s", line);
	}
	fclose(fp);
}

VMwareType::VMwareType(const char* prog_for_script, const char* scriptname, 
	const char* workingpath, ClassAd* ad) : 
	VMType(prog_for_script, scriptname, workingpath, ad)
{
	m_vmtype = CONDOR_VM_UNIVERSE_VMWARE;

	//m_cputime_before_suspend = 0;

	m_need_snapshot = false;
	m_restart_with_ckpt = false;
	m_vmware_transfer = false;
	m_vmware_snapshot_disk = true;

	// delete lock files
	deleteLockFiles();
}

VMwareType::~VMwareType()
{
	Shutdown();

	if( getVMStatus() != VM_STOPPED ) {
		// To make sure the process for VM exits.
		killVM();
	}
	setVMStatus(VM_STOPPED);
}

void
VMwareType::Config()
{
	// Nothing to do
}

void
VMwareType::adjustConfigDiskPath()
{
	if( m_configfile.empty() || 
			(check_vm_read_access_file(m_configfile.c_str()) == false)) {
		return;
	}

	if( m_vmware_dir.empty() ) {
		return;
	}

	std::string iwd;
	if( m_classAd.LookupString(ATTR_ORIG_JOB_IWD, iwd) == 1 ) {
		if( strcmp(iwd.c_str(), m_vmware_dir.c_str()) == 0 ) {
			vmprintf(D_FULLDEBUG, "job_iwd(%s) is the same to vmware dir "
					"so we will still use basename for parent disk of snapshot disk\n", 
					iwd.c_str());
			return;
		}
	}

	// read config file to find snapshot files
	FILE *fp = NULL;
	fp = safe_fopen_wrapper_follow(m_configfile.c_str(), "r");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper file(%s) : "
				"safe_fopen_wrapper returns %s\n", m_configfile.c_str(), strerror(errno));
		return;
	}

	char linebuf[2048];
	std::string one_line;
	std::string name;
	std::string value;
	StringList snapshot_disks;

	while( fgets(linebuf, 2048, fp) ) {
		one_line = linebuf;
		trim(one_line);

		if( one_line.length() == 0 || one_line[0] == '#' ) {
			/* Skip over comments */
			continue;
		}

		parse_param_string(one_line.c_str(), name, value, true);

		if( name.length() == 0 ) {
			continue;
		}

		if( !strncasecmp(name.c_str(), "scsi", strlen("scsi")) ||
				!strncasecmp(name.c_str(), "ide", strlen("ide"))) {
			std::string tmp_name = name;
			lower_case(tmp_name);
			if( tmp_name.find( ".filename", 0 ) != std::string::npos ) {
				if( has_suffix(value.c_str(), ".vmdk") && 
						(fullpath(value.c_str()) == false) ) {
					snapshot_disks.append(value.c_str());
				}
			}
		}
	}
	fclose(fp);

	// Read main_snapshot vmdk file to find parent disk path
	StringList parent_filenames;
	char *one_file = NULL;
	snapshot_disks.rewind();
	while( (one_file = snapshot_disks.next()) != NULL ) {
		change_snapshot_vmdk_file(one_file, true, m_vmware_dir.c_str(), parent_filenames);
	}

	// Change vmsd file
	size_t pos;
	std::string vmsd_file(m_configfile);
	pos = vmsd_file.find(VMWARE_TMP_CONFIG_SUFFIX);
	if( pos != std::string::npos ) {
		vmsd_file.replace(pos, std::string::npos, "_condor.vmsd");
	}
	change_snapshot_vmsd_file(vmsd_file.c_str(), &parent_filenames, true, m_vmware_dir.c_str());
}

void
VMwareType::deleteLockFiles()
{
	// Delete unnecessary files such as lock files
#define VMWARE_WRITELOCK_SUFFIX	".WRITELOCK"	
#define VMWARE_READLOCK_SUFFIX	".READLOCK"	

	const char *tmp_file = NULL;
	m_initial_working_files.rewind();
	while( (tmp_file = m_initial_working_files.next()) != NULL ) {
		if( has_suffix(tmp_file, VMWARE_WRITELOCK_SUFFIX) ||
			has_suffix(tmp_file, VMWARE_READLOCK_SUFFIX)) {
			IGNORE_RETURN unlink(tmp_file);
			m_initial_working_files.deleteCurrent();
		}else if( has_suffix(tmp_file, ".vmdk") ) {
			// modify permission for vmdk files
			IGNORE_RETURN chmod(tmp_file, VMWARE_VMDK_FILE_PERM);
		}
	}

	// delete entries for these lock files
	m_transfer_intermediate_files.rewind();
	while( (tmp_file = m_transfer_intermediate_files.next()) != NULL ) {
		if( has_suffix(tmp_file, VMWARE_WRITELOCK_SUFFIX) ||
			has_suffix(tmp_file, VMWARE_READLOCK_SUFFIX)) {
			m_transfer_intermediate_files.deleteCurrent();
		}
	}
	m_transfer_input_files.rewind();
	while( (tmp_file = m_transfer_input_files.next()) != NULL ) {
		if( has_suffix(tmp_file, VMWARE_WRITELOCK_SUFFIX) ||
			has_suffix(tmp_file, VMWARE_READLOCK_SUFFIX)) {
			m_transfer_input_files.deleteCurrent();
		}
	}
}

bool 
VMwareType::findCkptConfig(std::string &vmconfig)
{
	if( m_transfer_intermediate_files.isEmpty()) {
		return false;
	}

	int file_length = 0;
	int config_length = strlen(VMWARE_TMP_TEMPLATE) + 
		strlen(VMWARE_TMP_CONFIG_SUFFIX); // vmXXXXX_condor.vmx
	char *tmp_file = NULL;
	const char *tmp_base = NULL;
	m_transfer_intermediate_files.rewind();
	while( (tmp_file = m_transfer_intermediate_files.next()) != NULL ) {
		tmp_base = condor_basename(tmp_file);
		file_length = strlen(tmp_base);
		if( (file_length == config_length ) && 
				has_suffix(tmp_file, VMWARE_TMP_CONFIG_SUFFIX) ) {
			// file has the ending suffix of "_condor.vmx"
			// This is the vm config file for checkpointed files
			if( check_vm_read_access_file(tmp_file) ) {
				vmconfig = tmp_file;
				return true;
			}else {
				vmprintf(D_ALWAYS, "Cannot read the vmware config file "
						"for checkpointed files\n");
				return false;
			}
		}
	}
	return false;
}

bool 
VMwareType::adjustCkptConfig(const char* vmconfig)
{
	if( !vmconfig ) {
		return false;
	}

	FILE *fp = NULL;
	char linebuf[2048];
	std::string tmp_line;
	StringList configVars;

	fp = safe_fopen_wrapper_follow(vmconfig, "r");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper ckpt vmx file(%s) : "
				"safe_fopen_wrapper returns %s\n", vmconfig, strerror(errno));
		return false;
	}

	// Read all lines
	bool in_local_param = false;
	while( fgets(linebuf, 2048, fp) ) {
		std::string one_line(linebuf);
		chomp(one_line); 

		// remove local parameters between VMWARE_VM_CONFIG_LOCAL_PARAMS_START 
		// and VMWARE_VM_CONFIG_LOCAL_PARAMS_END
		if( !strncasecmp(one_line.c_str(),
		                 VMWARE_LOCAL_SETTINGS_START_MARKER,
		                 strlen(VMWARE_LOCAL_SETTINGS_START_MARKER)) )
		{
			in_local_param = true;
			continue;
		}
		if( !strncasecmp(one_line.c_str(),
		                 VMWARE_LOCAL_SETTINGS_END_MARKER,
		                 strlen(VMWARE_LOCAL_SETTINGS_END_MARKER)) )
		{
			in_local_param = false;
			continue;
		}

		if( in_local_param ) {
			continue;
		}

		// adjust networking type
		if( m_vm_networking ) {
			if( !strncasecmp(one_line.c_str(), "ethernet0.connectionType", 
						strlen("ethernet0.connectionType")) ) {

				std::string networking_type;
				std::string tmp_string;
				std::string tmp_string2;

				tmp_string2 = m_vm_networking_type;
				upper_case(tmp_string2);

				formatstr(tmp_string, "VMWARE_%s_NETWORKING_TYPE", tmp_string2.c_str());

				char *net_type = param(tmp_string.c_str());
				if( net_type ) {
					networking_type = delete_quotation_marks(net_type);
					free(net_type);
				}else {
					net_type = param("VMWARE_NETWORKING_TYPE");
					if( net_type ) {
						networking_type = delete_quotation_marks(net_type);
						free(net_type);
					}else {
						// default networking type is nat
						networking_type = "nat";
					}
				}

				formatstr(tmp_line, "ethernet0.connectionType = \"%s\"", 
						networking_type.c_str());
				configVars.append(tmp_line.c_str());
				continue;
			}
		}

		configVars.append(one_line.c_str());
	}
	fclose(fp);
	fp = NULL;

	fp = safe_fopen_wrapper_follow(vmconfig, "w");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper ckpt vmx file(%s) : "
				"safe_fopen_wrapper returns %s\n", vmconfig, strerror(errno));
		return false;
	}

	// write config parameters
	configVars.rewind();
	char *oneline = NULL;
	while( (oneline = configVars.next()) != NULL ) {
		if( fprintf(fp, "%s\n", oneline) < 0 ) {
			vmprintf(D_ALWAYS, "failed to fprintf in adjustCkptConfig (%s:%s)\n",
					vmconfig, strerror(errno));
			fclose(fp);
			return false;
		}
	}

	// Insert local parameters
	if (!write_local_settings_from_file(fp,
	                                    VMWARE_LOCAL_SETTINGS_PARAM,
	                                    VMWARE_LOCAL_SETTINGS_START_MARKER,
	                                    VMWARE_LOCAL_SETTINGS_END_MARKER))
	{
		vmprintf(D_ALWAYS,
		         "failed to add local settings in adjustCkptConfig\n");
		fclose(fp);
		return false;
	}

	fclose(fp);

	// change permission
	int retval = chmod(vmconfig, VMWARE_VMX_FILE_PERM);
	if( retval < 0 ) {
		vmprintf(D_ALWAYS, "Failed to chmod %s\n", vmconfig);
		return false;
	}
	return true;
}

static bool starts_with(const char * str, const char * pre)
{
	size_t cp = strlen(pre);
	if (cp <= 0)
		return false;

	size_t cs = strlen(str);
	if (cs < cp)
		return false;

	for (size_t ix = 0; ix < cp; ++ix) {
		if (str[ix] != pre[ix]) {
			return false;
		}
	}
	return true;
}

static bool starts_with_ignore_case(const char * str, const char * pre)
{
	size_t cp = strlen(pre);
	if (cp <= 0)
		return false;

	size_t cs = strlen(str);
	if (cs < cp)
		return false;

	for (size_t ix = 0; ix < cp; ++ix) {
		if (str[ix] != pre[ix]) {
			if (_tolower(str[ix]) != _tolower(pre[ix]))
				return false;
		}
	}
	return true;
}

bool
VMwareType::readVMXfile(const char *filename, const char *dirpath)
{
	FILE *fp = NULL;
	char linebuf[2048];
	StringList strip_devices;
	StringList config_lines;
	StringList working_files;
	std::string tmp_line;
	std::string name;
	std::string value;

	m_configVars.clearAll();
	m_result_msg = "";
			
	// Find all files in the working directory
	find_all_files_in_dir(m_workingpath.c_str(), working_files, true);

	fp = safe_fopen_wrapper_follow(filename, "r");
	if( !fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper vmware vmx file(%s) : "
				"safe_fopen_wrapper returns %s\n", filename, strerror(errno));
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_VMWARE_VMX_NOT_FOUND;
		return false;
	}

	size_t pos = 0;

	// We will find cdrom devices first.
	while( fgets(linebuf, 2048, fp) ) {
		std::string one_line(linebuf);
		trim(one_line);

		if( one_line.length() == 0 || one_line[0] == '#' ) {
			/* Skip over comments */
			continue;
		}

		parse_param_string(one_line.c_str(), name, value, true);

		if( name.length() == 0 ) {
			continue;
		}

		std::string tmp_name = name;
		lower_case(tmp_name);
		if( tmp_name.find( ".devicetype", 0 ) != std::string::npos ) {
			// find cdrom & floppy devices that are not image files
#define CDROM_TYPE1		"atapi-cdrom"
#define CDROM_TYPE2		"cdrom-raw"
//#define CDROM_TYPE3		"cdrom-image" // TJ: 11/27/2013 no need to strip cdrom-images
#define FLOPPY_DEVICE	"device"
			if( (strcasecmp(value.c_str(), CDROM_TYPE1) == 0 ) ||
				(strcasecmp(value.c_str(), CDROM_TYPE2) == 0 ) ||
				(starts_with(tmp_name.c_str(), "floppy")
				 && (MATCH == strcasecmp(value.c_str(), FLOPPY_DEVICE)))) {
				pos = name.find('.');
				if( pos > 0 && pos != std::string::npos ) {
					name.erase(pos);
					strip_devices.append(name.c_str());
					continue;
				}
			}
		}
		// parallel and serial references to physical devices
		// have either autodetect == true, or filename is the name of a phys device.
		if (starts_with_ignore_case(name.c_str(), "serial") ||
			starts_with_ignore_case(name.c_str(), "parallel")) {
			bool strip = false;
			pos = name.find('.');
			if (pos > 0 && pos != std::string::npos) {
				const char * field = name.c_str()+pos+1;
				if (MATCH == strcasecmp(field, "autodetect")) {
					strip = (MATCH == strcasecmp(value.c_str(), "true"));
				} else if (MATCH == strcasecmp(field, "filename")) {
					#ifdef WIN32
					strip = starts_with_ignore_case(value.c_str(), "com") || starts_with_ignore_case(value.c_str(), "lpt");
					#else
					strip = starts_with(value.c_str(), "/dev/");
					#endif
				}
				name.erase(pos);
			}
			if (strip) {
				strip_devices.append(name.c_str());
				continue;
			}
		}

		config_lines.append(one_line.c_str());
	}
	fclose(fp);

	char *line = NULL;
	char *devn = NULL;
	bool is_strip_dev = false;
	config_lines.rewind();
	while( (line = config_lines.next()) != NULL ) {
	
		is_strip_dev = false;
		// delete all lines for stripped device
		strip_devices.rewind();
		while( (devn = strip_devices.next()) != NULL ) {
			if( strncasecmp(line, devn, strlen(devn)) == 0 ) {
				// This is a line for device we want to strip.
				is_strip_dev = true;
				break;
			}
		}
		if (is_strip_dev) {
			continue;
		}

		parse_param_string(line, name, value, true);

		if( name.length() == 0 ) {
			continue;
		}

		if( !strncasecmp(line, "config.", strlen("config.")) ||
				!strncasecmp(line, "virtualHW.", strlen("virtualHW.")) ||
				!strncasecmp(line, "guestOS", strlen("guestOS"))) {
			m_configVars.append(line);
		}else if( !strncasecmp(line, "scsi", strlen("scsi")) ||
				!strncasecmp(line, "ide", strlen("ide"))) {
			std::string tmp_name = name;
			lower_case(tmp_name);
			if( tmp_name.find( ".filename", 0 ) != std::string::npos ) {

				// Adjust filename
				const char *tmp_base_name = condor_basename(value.c_str());
				if( !tmp_base_name ) {
					m_configVars.append(line);
					continue;
				}

				if( filelist_contains_file(value.c_str(), 
							&working_files, true) ) {
					// file is transferred 
					if( fullpath(value.c_str()) ) {
						// we use basename instead of fullname
						formatstr(tmp_line, "%s = \"%s\"", name.c_str(), 
								tmp_base_name );
						m_configVars.append(tmp_line.c_str());
					}else {
						m_configVars.append(line);
					}
				}else {
					// file is not transferred, so we need to use fullname
					if( fullpath(value.c_str()) ) {
						// the filename is already fullname
						if( check_vm_read_access_file(value.c_str()) == false ) {
							vmprintf(D_ALWAYS, "file(%s) in a vmx file cannot "
									"be read\n", value.c_str());
							m_result_msg = VMGAHP_ERR_JOBCLASSAD_VMWARE_VMX_ERROR;
							return false;
						}
						m_configVars.append(line);
					}else {
						// we create fullname with given dirpath
						std::string tmp_fullname;
						formatstr(tmp_fullname, "%s%c%s", dirpath, 
								DIR_DELIM_CHAR, tmp_base_name);

						formatstr(tmp_line, "%s = \"%s\"", name.c_str(), 
								tmp_fullname.c_str());

						if( !(*dirpath) || check_vm_read_access_file(tmp_fullname.c_str()) 
								== false ) {
							vmprintf(D_ALWAYS, "file(%s) in a vmx file cannot "
									"be read\n", tmp_fullname.c_str());
							m_result_msg = VMGAHP_ERR_JOBCLASSAD_VMWARE_VMX_ERROR;
							return false;
						}
						m_configVars.append(tmp_line.c_str());
					}
				}

				// set writeThrough flag to TRUE
				// It means to disable write cache
				pos = tmp_name.find('.', 0);
				if( pos != std::string::npos ) {
					tmp_name.erase(pos);
					formatstr(tmp_line, "%s.writeThrough = \"TRUE\"", tmp_name.c_str());
					m_configVars.append(tmp_line.c_str());
				}

				continue;
			}else if( (tmp_name.find(".mode", 0 ) != std::string::npos) && (value.length() > 0)) {
				std::string tmp_value = value;
				lower_case(tmp_value);
				if( tmp_value.find( "independent", 0 ) != std::string::npos ) {
					if( !m_vmware_transfer ) {
						// In VMware, independent disks are not affected 
						// by snapshots. In a shared filesystem, 
						// We always use snapshot disks in order that 
						// multiple jobs shares the same disk.
						// We disable independent disk mode 
						// so that all disks will be affected by snapshots
						continue;
					}
				}
			}else if( tmp_name.find( ".writethrough", 0 ) != std::string::npos) {
				// We already set writeThrough to TRUE
				// So skiping this line
				continue;
			}

			m_configVars.append(line);
		} else if( !strncasecmp(line, "ethernet0.virtualDev", strlen("ethernet0.virtualDev")) ) {
			m_configVars.append(line);
		} else if( !strncasecmp(line, "floppy", strlen("floppy")) ) {
			m_configVars.append(line);
		} else if( !strncasecmp(line, "parallel", strlen("parallel")) ) {
			m_configVars.append(line);
		} else if( !strncasecmp(line, "serial", strlen("serial")) ) {
			m_configVars.append(line);
		}
	}

	return true;
}

bool 
VMwareType::CombineDisks()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::CombineDisks\n");

	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("commit");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner);
	if( result != 0 ) {
		return false;
	}

	return true;
}

bool
VMwareType::Unregister()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Unregister\n");

	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("unregister");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner);
	if( result != 0 ) {
		return false;
	}
	return true;
}

bool
VMwareType::Snapshot()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Snapshot\n");
	
	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("snapshot");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner, &cmd_out);
	if( result != 0 ) {
		char *temp = cmd_out.print_to_delimed_string("/");
		m_result_msg = temp ? temp : "";
		free( temp );
		return false;
	}

#if defined(LINUX)	
	// To avoid lazy-write behavior to disk
	sync();
#endif

	return true;
}

bool 
VMwareType::Start()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Start\n");

	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() != VM_STOPPED ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}
		
	if( m_restart_with_ckpt ) {
		m_restart_with_ckpt = false;
		m_need_snapshot = false;
		bool res = Start();
		if( res ) {
			vmprintf(D_ALWAYS, "Succeeded to restart with checkpointed files\n");
			return true;
		}else {
			// Failed to restart with checkpointed files
			vmprintf(D_ALWAYS, "Failed to restart with checkpointed files\n");
			vmprintf(D_ALWAYS, "So, we will try to create a new configuration file\n");

			deleteNonTransferredFiles();
			m_configfile = "";
			m_restart_with_ckpt = false;

			if( CreateConfigFile() == false ) {
				vmprintf(D_ALWAYS, "Failed to create a new configuration files\n");
				return false;
			}

			// Succeeded to create a configuration file
			// Keep going..
		}
	}

	if( m_need_snapshot ) {
		if( Snapshot() == false ) {
			Unregister();
			return false;
		}
	}

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("start");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner, &cmd_out);
	if( result != 0 ) {
		Unregister();
		char *temp = cmd_out.print_to_delimed_string("/");
		m_result_msg = temp ? temp : "";
		free( temp );
		return false;
	}

	// Got Pid result
	m_vm_pid = 0;
	cmd_out.rewind();
	const char *pid_line;
	while ( (pid_line = cmd_out.next()) ) {
		if ( sscanf( pid_line, "PID=%d", &m_vm_pid ) == 1 ) {
			if ( m_vm_pid <= 0 ) {
				m_vm_pid = 0;
			}
			break;
		}
	}

	setVMStatus(VM_RUNNING);
	m_start_time = time(NULL);
    //m_cpu_time = 0;
	return true;
}

bool
VMwareType::ShutdownFast()
{
	static bool sent_signal = false;
	vmprintf(D_FULLDEBUG, "Inside VMwareType::ShutdownFast\n");

	bool ret = false;
	if( m_vm_pid > 0 && daemonCore ) {
		if( !sent_signal ) {
			vmprintf(D_FULLDEBUG, "Sending Kill signal to process(pid=%d)\n", m_vm_pid);
			ret = daemonCore->Send_Signal(m_vm_pid, SIGKILL);
			if( ret ) {
				// success to send signal
				m_vm_pid = 0;
				sleep(1);
			}
			sent_signal = true;
		}
	}
	return ret;
}

bool
VMwareType::ShutdownGraceful()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::ShutdownGraceful\n");

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("stop");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner);
	if( result != 0 ) {
		return false; 
	}

	m_vm_pid = 0;
	setVMStatus(VM_STOPPED);
	return true;
}


bool
VMwareType::Shutdown()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Shutdown\n");

	if( (m_scriptname.length() == 0) ||
			(m_configfile.length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_STOPPED ) {
		if( m_self_shutdown ) {
			if( m_vmware_transfer && m_vmware_snapshot_disk 
					&& !m_vm_no_output_vm ) {
				// The path of parent disk in a snapshot disk 
				// used basename because all parent disk files 
				// were transferred. So we need to replace 
				// the path with the path on submit machine.
				priv_state old_priv = set_user_priv();
				adjustConfigDiskPath();
				set_priv( old_priv );
			}
			Unregister();

			if( m_vm_no_output_vm ) {
				// A job user doesn't want to get back VM files.
				// So we will delete all files in working directory.
				m_delete_working_files = true;
				m_is_checkpointed = false;
			}
		}
		// We here set m_self_shutdown to false
		// So, above functions will not be called twice
		m_self_shutdown = false;
		return true;
	}

	if( getVMStatus() == VM_SUSPENDED ) {
		// Unregistering ...
		Unregister();
	}
	
	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	if( getVMStatus() == VM_RUNNING ) {
		if( ShutdownGraceful() == false ) {
			vmprintf(D_ALWAYS, "ShutdownGraceful failed ..\n");
			// We failed to stop a running VM gracefully.
			// So we will try to destroy the VM forcedly.
			if( killVM() == false ) {
				vmprintf(D_ALWAYS, "killVM failed ..\n");
				// We failed again. So final step is 
				// to try kill process for VM directly.
				ShutdownFast();
				Unregister();
			}
		}
		// Now we don't need to keep working files any more
		m_delete_working_files = true;
		m_is_checkpointed = false;
	}
	
	m_vm_pid = 0;
	setVMStatus(VM_STOPPED);
	m_stop_time = time(NULL);
	return true;
}

bool
VMwareType::Checkpoint()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Checkpoint\n");

	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_STOPPED ) {
		vmprintf(D_ALWAYS, "Checkpoint is called for a stopped VM\n");
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	if( !m_vm_checkpoint ) {
		vmprintf(D_ALWAYS, "Checkpoint is not supported.\n");
		m_result_msg = VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT;
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	// This function cause a running VM to be suspended.
	if( createCkptFiles() == false ) { 
		m_result_msg = VMGAHP_ERR_VM_CANNOT_CREATE_CKPT_FILES;
		vmprintf(D_ALWAYS, "failed to create checkpoint files\n");
		return false;
	}

	// VM is suspended for checkpoint.
	// so there is no process for VM.
	m_vm_pid = 0;

	return true;
}

bool
VMwareType::ResumeFromSoftSuspend()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::ResumeFromSoftSuspend\n");
	if( m_is_soft_suspended ) {
		if( m_vm_pid > 0 ) {
			// Send SIGCONT to a process for VM
			if( daemonCore->Send_Signal(m_vm_pid, SIGCONT) == false ) {
				// Sending SIGCONT failed
				vmprintf(D_ALWAYS, "Sending SIGCONT to process[%d] failed in "
						"VMwareType::ResumeFromSoftSuspend\n", m_vm_pid);
				return false;
			}
		}
		m_is_soft_suspended = false;
	}
	return true;
}

bool 
VMwareType::SoftSuspend()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::SoftSuspend\n");

	if( m_is_soft_suspended ) {
		return true;
	}

	if( getVMStatus() != VM_RUNNING ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	if( m_vm_pid > 0 ) {
		// Send SIGSTOP to a process for VM
		if( daemonCore->Send_Signal(m_vm_pid, SIGSTOP) ) {
			m_is_soft_suspended = true;
			return true;
		}
	}

	// Failed to suspend a VM softly.
	// Instead of soft suspend, we use hard suspend.
	vmprintf(D_ALWAYS, "SoftSuspend failed, so try hard Suspend instead!.\n");
	return Suspend();
}

bool 
VMwareType::Suspend()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Suspend\n");

	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( getVMStatus() == VM_SUSPENDED ) {
		return true;
	}

	if( getVMStatus() != VM_RUNNING ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("suspend");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner, &cmd_out);
	if( result != 0 ) {
		char *temp = cmd_out.print_to_delimed_string("/");
		m_result_msg = temp ? temp : "";
		free( temp );
		return false;
	}

	// Suspend succeeds. So there is no process for VM.
	m_vm_pid = 0;
	setVMStatus(VM_SUSPENDED);
	//m_cputime_before_suspend += m_cpu_time;
	//m_cpu_time = 0;
	return true;
}

bool 
VMwareType::Resume()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Resume\n");

	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	if( getVMStatus() == VM_RUNNING ) {
		return true;
	}

	if( getVMStatus() != VM_SUSPENDED ) {
		m_result_msg = VMGAHP_ERR_VM_INVALID_OPERATION;
		return false;
	}

	m_is_checkpointed = false;

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("resume");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner, &cmd_out);
	if( result != 0 ) {
		char *temp = cmd_out.print_to_delimed_string("/");
		m_result_msg = temp ? temp : "";
		free( temp );
		return false;
	}

	// Got Pid result
	m_vm_pid = 0;
	cmd_out.rewind();
	const char *pid_line;
	while ( (pid_line = cmd_out.next()) ) {
		if ( sscanf( pid_line, "PID=%d", &m_vm_pid ) == 1 ) {
			if ( m_vm_pid <= 0 ) {
				m_vm_pid = 0;
			}
			break;
		}
	}

	setVMStatus(VM_RUNNING);
	return true;
}

bool
VMwareType::Status()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::Status\n");

	if( (m_scriptname.length() == 0) ||
			(m_configfile.length() == 0)) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	if( m_is_soft_suspended ) {
		// If a VM is softly suspended, 
		// we cannot get info about the VM by using script
		m_result_msg = VMGAHP_STATUS_COMMAND_STATUS;
		m_result_msg += "=";
		m_result_msg += "SoftSuspended";
		return true;
	}

	// Check the last time when we executed status.
	// If the time is in 10 seconds before current time, 
	// We will not execute status again.
	// Maybe this case may happen when it took long time 
	// to execute the last status.
	long diff_seconds = 0;

	diff_seconds = time(NULL) - m_last_status_time;

	if( (diff_seconds < 10) && !m_last_status_result.empty() ) {
		m_result_msg = m_last_status_result;
		return true;
	}

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	if( m_vm_networking ) {
		systemcmd.AppendArg("getvminfo");
	}else {
		systemcmd.AppendArg("status");
	}
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner, &cmd_out);
	if( result != 0 ) {
		char *temp = cmd_out.print_to_delimed_string("/");
		m_result_msg = temp ? temp : "";
		free( temp );
		return false;
	}

	// Got result
	const char *next_line;
	std::string one_line;
	std::string name;
	std::string value;

	std::string vm_status;
	int vm_pid = 0;
	float cputime = 0;
	cmd_out.rewind();
	while( (next_line = cmd_out.next()) != NULL ) {
		one_line = next_line;
		trim(one_line);

		if( one_line.length() == 0 ) {
			continue;
		}

		if( one_line[0] == '#' ) {
			/* Skip over comments */
			continue;
		}

		parse_param_string(one_line.c_str(), name, value, true);
		if( !name.length() || !value.length() ) {
			continue;
		}
		
		if( !strcasecmp(name.c_str(), VMGAHP_STATUS_COMMAND_CPUTIME)) {
			cputime = (float)strtod(value.c_str(), (char **)NULL);
			if( cputime <= 0 ) {
				cputime = 0;
			}
			continue;
		}

		if( !strcasecmp(name.c_str(), VMGAHP_STATUS_COMMAND_STATUS)) {
			vm_status = value;
			continue;
		}
		if( !strcasecmp(name.c_str(), VMGAHP_STATUS_COMMAND_PID) ) {
			vm_pid = (int)strtol(value.c_str(), (char **)NULL, 10);
			if( vm_pid <= 0 ) {
				vm_pid = 0;
			}
			continue;
		}
		if( m_vm_networking ) {
			if( !strcasecmp(name.c_str(), VMGAHP_STATUS_COMMAND_MAC) ) {
				m_vm_mac = value;
				continue;
			}
			if( !strcasecmp(name.c_str(), VMGAHP_STATUS_COMMAND_IP) ) {
				m_vm_ip = value;
				continue;
			}
		}
	}

	if( !vm_status.length() ) {
		m_result_msg = VMGAHP_ERR_CRITICAL;
		return false;
	}

	m_result_msg = "";

	if( m_vm_networking ) {
		if( m_vm_mac.empty() == false ) {
			if( m_result_msg.empty() == false ) {
				m_result_msg += " ";
			}
			m_result_msg += VMGAHP_STATUS_COMMAND_MAC;
			m_result_msg += "=";
			m_result_msg += m_vm_mac;
		}

		if( m_vm_ip.empty() == false ) {
			if( m_result_msg.empty() == false ) {
				m_result_msg += " ";
			}
			m_result_msg += VMGAHP_STATUS_COMMAND_IP;
			m_result_msg += "=";
			m_result_msg += m_vm_ip;
		}
	}

	if( m_result_msg.empty() == false ) {
		m_result_msg += " ";
	}

	m_result_msg += VMGAHP_STATUS_COMMAND_STATUS;
	m_result_msg += "=";

	if( strcasecmp(vm_status.c_str(), "Running") == 0 ) {
		setVMStatus(VM_RUNNING);

		if( !vm_pid ) {
			// Retry to get pid
			getPIDofVM(vm_pid);
		}
		m_vm_pid = vm_pid;

		formatstr_cat( m_result_msg, "Running %s=%d",
		               VMGAHP_STATUS_COMMAND_PID, m_vm_pid );

		if( cputime > 0 ) {
			// Update vm running time
			m_cpu_time = cputime;

			formatstr_cat( m_result_msg, " %s=%f",
			               VMGAHP_STATUS_COMMAND_CPUTIME,
			               m_cpu_time );
			//               m_cpu_time + m_cputime_before_suspend );
		}

		return true;

	}else if( strcasecmp(vm_status.c_str(), "Suspended") == 0 ) {
		// VM is suspended
		setVMStatus(VM_SUSPENDED);
		m_vm_pid = 0;
		m_result_msg += "Suspended";
		return true;
	}else if( strcasecmp(vm_status.c_str(), "Stopped") == 0 ) {
		// VM is stopped
		m_vm_pid = 0;

		if( getVMStatus() == VM_SUSPENDED ) {
			m_result_msg += "Suspended";
			return true;
		}

		if( getVMStatus() == VM_RUNNING ) {
			m_self_shutdown = true;
		}

		m_result_msg += "Stopped";
		if( getVMStatus() != VM_STOPPED ) {
			setVMStatus(VM_STOPPED);
			m_stop_time = time(NULL);
		}
		return true;
	}else {
		// Woops, something is wrong
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}
	return true;
}

bool 
VMwareType::getPIDofVM(int &vm_pid)
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::getPIDofVM\n");

	vm_pid = 0;

	if( (m_scriptname.length() == 0) ||
		(m_configfile.length() == 0)) {
		return false;
	}

	if( getVMStatus() != VM_RUNNING ) {
		return false;
	}

	StringList cmd_out;

	ArgList systemcmd;
	systemcmd.AppendArg(m_prog_for_script);
	systemcmd.AppendArg(m_scriptname);
	systemcmd.AppendArg("getpid");
	systemcmd.AppendArg(m_configfile);

	int result = systemCommand(systemcmd, m_file_owner, &cmd_out);
	if( result != 0 ) {
		return false;
	}

	// Got Pid result
	cmd_out.rewind();
	const char *pid_line;
	while ( (pid_line = cmd_out.next()) ) {
		if ( sscanf( pid_line, "PID=%d", &m_vm_pid ) == 1 ) {
			if ( m_vm_pid <= 0 ) {
				m_vm_pid = 0;
			}
			return true;
		}
	}
	return false;
}

bool
VMwareType::CreateConfigFile()
{
	std::string tmp_config_name;

	m_result_msg = "";

	// Read common parameters for VM
	// and create the name of this VM
	if( parseCommonParamFromClassAd() == false ) {
		return false;
	}

	// Read the flag about transferring vmware files
	m_vmware_transfer = false;
	m_classAd.LookupBool(VMPARAM_VMWARE_TRANSFER, m_vmware_transfer);

	// Read the flag about snapshot disk
	m_vmware_snapshot_disk = true;
	m_classAd.LookupBool(VMPARAM_VMWARE_SNAPSHOTDISK, m_vmware_snapshot_disk);

	// Read the directory where vmware files are on a submit machine
	m_vmware_dir = "";
	m_classAd.LookupString(VMPARAM_VMWARE_DIR, m_vmware_dir);
	trim(m_vmware_dir);

	// Read the parameter of vmware vmx file
	if( m_classAd.LookupString(VMPARAM_VMWARE_VMX_FILE, m_vmware_vmx) != 1 ) {
		vmprintf(D_ALWAYS, "%s cannot be found in job classAd\n", 
							VMPARAM_VMWARE_VMX_FILE);
		m_result_msg = VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_VMX_PARAM;
		return false;
	}
	trim(m_vmware_vmx);

	// Read the parameter of vmware vmdks
	if( m_classAd.LookupString(VMPARAM_VMWARE_VMDK_FILES, m_vmware_vmdk) == 1 ) {
		trim(m_vmware_vmdk);
	}

	if( !m_vmware_transfer ) {
		// we use a shared filesystem
		// So we always use snapshot disks
		m_need_snapshot = true;
	}else {
		// Disk files are transferred 
		m_need_snapshot = m_vmware_snapshot_disk;
	}

	// Check whether this is re-starting after vacating or periodic checkpointing 
	if( m_transfer_intermediate_files.isEmpty() == false) {
		// We have checkpointed files
		// So, we don't need to create vm config file
		// Find the vm config file for checkpointed files
		std::string ckpt_config_file;
		if( findCkptConfig(ckpt_config_file) == false ) {
			vmprintf(D_ALWAYS, "Checkpoint files exist but "
					"cannot find the config file for them\n");
			// Delete all non-transferred files from submit machine
			deleteNonTransferredFiles();
			m_restart_with_ckpt = false;
		}else {
			// We found a valid vm configuration file with checkpointed files
			// Now, we need to adjust the configuration file, if necessary.
			if( adjustCkptConfig(ckpt_config_file.c_str()) == false ) {
				vmprintf(D_ALWAYS, "Failed to adjust vm config file(%s) for ckpt files "
						"in VMwareType::CreateConfigFile()\n", 
						ckpt_config_file.c_str());
				deleteNonTransferredFiles();
				m_restart_with_ckpt = false;
			}else {
				m_configfile = ckpt_config_file;
				m_need_snapshot = false;
				m_restart_with_ckpt = true;
				vmprintf(D_ALWAYS, "Found checkpointed files, "
						"so we start using them\n");
				return true;
			}
		}
	}

	// Create vm config file
	if( createTempFile(VMWARE_TMP_TEMPLATE, VMWARE_TMP_CONFIG_SUFFIX, 
				tmp_config_name) == false ) {
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	// Change file permission
	int retval = chmod(tmp_config_name.c_str(), VMWARE_VMX_FILE_PERM);
	if( retval < 0 ) {
		vmprintf(D_ALWAYS, "Failed to chmod %s in "
				"VMwareType::CreateConfigFile()\n", tmp_config_name.c_str());
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	// Read transferred vmx file
	std::string ori_vmx_file;
	formatstr(ori_vmx_file, "%s%c%s",m_workingpath.c_str(), 
			DIR_DELIM_CHAR, m_vmware_vmx.c_str());

	if( readVMXfile(ori_vmx_file.c_str(), m_vmware_dir.c_str()) 
			== false ) {
		IGNORE_RETURN unlink(tmp_config_name.c_str());
		return false;
	}

	// Add memsize to m_configVars
	std::string tmp_line;
	formatstr(tmp_line, "memsize = \"%d\"", m_vm_mem);
	m_configVars.append(tmp_line.c_str());

	// Add displyName to m_configVars
	formatstr(tmp_line, "displayName = \"%s\"", m_vm_name.c_str());
	m_configVars.append(tmp_line.c_str());

	// Add networking parameters to m_configVars
	if( m_vm_networking ) {
		std::string networking_type;
		std::string tmp_string; 
		std::string tmp_string2;

		tmp_string2 = m_vm_networking_type;
		upper_case(tmp_string2);

		formatstr(tmp_string, "VMWARE_%s_NETWORKING_TYPE", tmp_string2.c_str());

		char *net_type = param(tmp_string.c_str());
		if( net_type ) {
			networking_type = delete_quotation_marks(net_type);
			free(net_type);
		}else {
			net_type = param("VMWARE_NETWORKING_TYPE");
			if( net_type ) {
				networking_type = delete_quotation_marks(net_type);
				free(net_type);
			}else {
				// default networking type is nat
				networking_type = "nat";
			}
		}

		m_configVars.append("ethernet0.present = \"TRUE\"");
		formatstr(tmp_line, "ethernet0.connectionType = \"%s\"", 
				networking_type.c_str());
		m_configVars.append(tmp_line.c_str());
        if (!m_vm_job_mac.empty())
        {
            vmprintf(D_FULLDEBUG, "mac address is %s\n", m_vm_job_mac.c_str());
            m_configVars.append("ethernet0.addressType = \"static\"");
            formatstr(tmp_line, "ethernet0.address = \"%s\"", m_vm_job_mac.c_str());
            m_configVars.append(tmp_line.c_str());
            //**********************************************************************
            // LIMITATION: the mac address has to be in the range
            // 00:50:56:00:00:00 - 00:50:56:3F:FF:FF
            // This is a vmware limitation and I can't find a way to circumvent it.
            //**********************************************************************
        } else {
    		m_configVars.append("ethernet0.addressType = \"generated\"");
        }
	}

	// Add uuid option
	m_configVars.append("uuid.action = \"keep\"");

	// Don't create lock file for disks
	m_configVars.append("disk.locking = \"FALSE\"");

	FILE *config_fp = safe_fopen_wrapper_follow(tmp_config_name.c_str(), "w");
	if( !config_fp ) {
		vmprintf(D_ALWAYS, "failed to safe_fopen_wrapper vmware config file "
				"with write mode: safe_fopen_wrapper_follow(%s) returns %s\n", 
				tmp_config_name.c_str(), strerror(errno));

		IGNORE_RETURN unlink(tmp_config_name.c_str());
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	// write config parameters
	m_configVars.rewind();
	char *oneline = NULL;
	while( (oneline = m_configVars.next()) != NULL ) {
		if( fprintf(config_fp, "%s\n", oneline) < 0 ) {
			vmprintf(D_ALWAYS, "failed to fprintf in CreateConfigFile(%s:%s)\n",
					tmp_config_name.c_str(), strerror(errno));

			fclose(config_fp);
			IGNORE_RETURN unlink(tmp_config_name.c_str());
			m_result_msg = VMGAHP_ERR_INTERNAL;
			return false;
		}
	}

	if (!write_local_settings_from_file(config_fp,
	                                    VMWARE_LOCAL_SETTINGS_PARAM,
	                                    VMWARE_LOCAL_SETTINGS_START_MARKER,
	                                    VMWARE_LOCAL_SETTINGS_END_MARKER))
	{
		vmprintf(D_ALWAYS,
		         "failed to add local settings in CreateConfigFile\n");
		fclose(config_fp);
		IGNORE_RETURN unlink(tmp_config_name.c_str());
		m_result_msg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	fclose(config_fp);
	config_fp = NULL;

	if( m_use_script_to_create_config ) {
		// We will call the script program 
		// to create a configuration file for VM

		if( createConfigUsingScript(tmp_config_name.c_str()) == false ) {
			IGNORE_RETURN unlink(tmp_config_name.c_str());
			m_result_msg = VMGAHP_ERR_CRITICAL;
			return false;
		}
	}

	// set vm config file
	m_configfile = tmp_config_name;
	return true;
}

bool
VMwareType::createCkptFiles()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::createCkptFiles\n");

	// This function will suspend a running VM.
	if( getVMStatus() == VM_STOPPED ) {
		vmprintf(D_ALWAYS, "createCkptFiles is called for a stopped VM\n");
		return false;
	}

	if( getVMStatus() == VM_RUNNING ) {
		if( Suspend() == false ) {
			return false;
		}
	}

	if( getVMStatus() == VM_SUSPENDED ) {
		char *tmp_file = NULL;
		StringList ckpt_files;
		struct utimbuf timewrap;
		time_t current_time;

		find_all_files_in_dir(m_workingpath.c_str(), ckpt_files, true);

		ckpt_files.rewind();
		while( (tmp_file = ckpt_files.next()) != NULL ) {
			// In some systems such as Linux, mtime may not be updated 
			// after changes to files via mmap. For example, 
			// the mtime of VMware vmem file is not updated 
			// even after changes, because VMware uses the file via mmap.
			// So we manually update mtimes of some files.
			if( !has_suffix(tmp_file, ".vmdk") && 
					!has_suffix(tmp_file, ".iso") &&
					!has_suffix(tmp_file, ".log") &&
					!has_suffix(tmp_file, VMWARE_WRITELOCK_SUFFIX ) &&
					!has_suffix(tmp_file, VMWARE_READLOCK_SUFFIX ) &&
					strcmp(condor_basename(tmp_file), m_vmware_vmx.c_str())) {
				// We update mtime and atime of all files 
				// except vmdk, iso, log, lock files, cdrom file, and 
				// the original vmx file.
				current_time = time(NULL);
				timewrap.actime = current_time;
				timewrap.modtime = current_time;
				utime(tmp_file, &timewrap);
			}
		}

		// checkpoint succeeds
		m_is_checkpointed = true;
		return true;
	}

	return false;
}

bool 
VMwareType::checkVMwareParams(VMGahpConfig* config)
{
	char *config_value = NULL;
	std::string fixedvalue;

	if( !config ) {
		return false;
	}

	// find perl program
	config_value = param("VMWARE_PERL");
	if( !config_value ) {
		vmprintf(D_ALWAYS,
		         "\nERROR: 'VMWARE_PERL' not in configuration\n");
		return false;
	}
	fixedvalue = delete_quotation_marks(config_value);
	free(config_value);
	config->m_prog_for_script = fixedvalue;

	// find script program for VMware
	config_value = param("VMWARE_SCRIPT");
	if( !config_value ) {
		vmprintf(D_ALWAYS,
		         "\nERROR: 'VMWARE_SCRIPT' not in configuration\n");
		return false;
	}
	fixedvalue = delete_quotation_marks(config_value);
	free(config_value);

#if !defined(WIN32)
	struct stat sbuf;
	if( stat(fixedvalue.c_str(), &sbuf ) < 0 ) {
		vmprintf(D_ALWAYS, "\nERROR: Failed to access the script "
				"program for VMware:(%s:%s)\n", fixedvalue.c_str(),
				strerror(errno));
		return false;
	}

	// Other writable bit
	if( sbuf.st_mode & S_IWOTH ) {
		vmprintf(D_ALWAYS, "\nFile Permission Error: "
				"other writable bit is not allowed for \"%s\" "
				"due to security issues.\n", fixedvalue.c_str());
		return false;
	}

	// Other readable bit
	if( !(sbuf.st_mode & S_IROTH) ) {
		vmprintf(D_ALWAYS, "\nFile Permission Error: "
				"\"%s\" must be readable by anybody, because script program "
				"will be executed with user permission.\n", fixedvalue.c_str());
		return false;
	}
#endif

	// Can read script program?
	if( check_vm_read_access_file(fixedvalue.c_str()) == false ) {
		return false;
	}
	config->m_vm_script = fixedvalue;

	return true;
}

bool 
VMwareType::testVMware(VMGahpConfig* config)
{
	if( !config ) {
		return false;
	}

	if( VMwareType::checkVMwareParams(config) == false ) {
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(config->m_prog_for_script);
	systemcmd.AppendArg(config->m_vm_script);
	systemcmd.AppendArg("check");

	int result = systemCommand(systemcmd, PRIV_USER);
	if( result != 0 ) {
		vmprintf( D_ALWAYS, "VMware script check failed:\n" );
		return false;
	}

	return true;
}

bool 
VMwareType::killVM()
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::killVM\n");

	if( (m_scriptname.length() == 0) ||
			(m_configfile.length() == 0)) {
		return false;
	}

	// If a VM is soft suspended, resume it first.
	ResumeFromSoftSuspend();

	return killVMFast(m_prog_for_script.c_str(), m_scriptname.c_str(), 
			m_configfile.c_str());
}

bool 
VMwareType::killVMFast(const char* prog_for_script, const char* script, 
		const char* matchstring, bool is_root /*false*/)
{
	vmprintf(D_FULLDEBUG, "Inside VMwareType::killVMFast\n");

	if( !script || (script[0] == '\0') ||
			!matchstring || (matchstring[0] == '\0') ) {
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(prog_for_script);
	systemcmd.AppendArg(script);
	systemcmd.AppendArg("killvm");
	systemcmd.AppendArg(matchstring);

	int result = systemCommand(systemcmd, is_root ? PRIV_ROOT : PRIV_USER);
	if( result != 0 ) {
		return false;
	}
	return true;
}
