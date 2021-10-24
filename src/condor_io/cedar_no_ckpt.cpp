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



/* 
   This file contains implementations of functions we need in the
   regular libcedar.a that we do NOT want to link into standard
   universe jobs with libcondorsyscall.a.  Any functions implemented
   here need to be added to condor_syscall_lib/cedar_no_ckpt_stubs.C
   that calls EXCEPT() or whatever is appropriate.  This way, we can
   add functionality to CEDAR that we need/use outside of the syscall
   library without causing trouble inside the standard universe code.
*/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "basename.h"
#include "directory.h"
#include "globus_utils.h"
#include "condor_auth_x509.h"
#include "condor_config.h"
#include "ccb_client.h"
#include "condor_sinful.h"
#include "shared_port_client.h"
#include "condor_netdb.h"
#include "internet.h"
#include "ipv6_hostname.h"
#include "condor_fsync.h"
#include "dc_transfer_queue.h"
#include "limit_directory_access.h"

#ifdef WIN32
#include <mswsock.h>	// For TransmitFile()
#endif

#ifdef HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

const unsigned int PUT_FILE_EOM_NUM = 666;

// This special file descriptor number must not be a valid fd number.
// It is used to make get_file() consume transferred data without writing it.
const int GET_FILE_NULL_FD = -10;

const size_t OLD_FILE_BUF_SZ = 65536;
const size_t AES_FILE_BUF_SZ = 262144;

int
ReliSock::get_file( filesize_t *size, const char *destination,
					bool flush_buffers, bool append, filesize_t max_bytes,
					DCTransferQueue *xfer_q)
{
	int fd;
	int result;
	int flags = O_WRONLY | _O_BINARY | _O_SEQUENTIAL | O_LARGEFILE;

	if ( append ) {
		flags |= O_APPEND;
	}
	else {
		flags |= O_CREAT | O_TRUNC;
	}

	if (allow_shadow_access(destination)) {
		// Open the file
		errno = 0;
		fd = ::safe_open_wrapper_follow(destination, flags, 0600);
	}
	else {
		fd = -1;
		errno = EACCES;
	}

	// Handle open failure; it's bad....
	if ( fd < 0 )
	{
		int saved_errno = errno;
#ifndef WIN32 /* Unix */
		if ( errno == EMFILE ) {
			_condor_fd_panic( __LINE__, __FILE__ ); /* This calls dprintf_exit! */
		}
#endif
		dprintf(D_ALWAYS,
				"get_file(): Failed to open file %s, errno = %d: %s.\n",
				destination, saved_errno, strerror(saved_errno) );

			// In order to remain in a well-defined state on the wire
			// protocol, read and throw away the file data.
			// We're not going to write the data, so don't try to append.
		result = get_file( size, GET_FILE_NULL_FD, flush_buffers, /*append*/ false, max_bytes, xfer_q );
		if( result<0 ) {
				// Failure to read (and throw away) data indicates that
				// we are in an undefined state on the wire protocol
				// now, so return that type of failure, rather than
				// the well-defined failure code for OPEN_FAILED.
			return result;
		}

		errno = saved_errno;
		return GET_FILE_OPEN_FAILED;
	} 

	dprintf( D_FULLDEBUG,
			 "get_file(): going to write to filename %s\n",
			 destination);

	result = get_file( size, fd,flush_buffers, append, max_bytes, xfer_q);

	if(::close(fd)!=0) {
		dprintf(D_ALWAYS,
				"ReliSock: get_file: close failed, errno = %d (%s)\n",
				errno, strerror(errno));
		result = -1;
	}

	if(result<0) {
		if (unlink(destination) < 0) {
			dprintf(D_FULLDEBUG, "get_file(): failed to unlink file %s errno = %d: %s.\n", 
			        destination, errno, strerror(errno));
		}
	}
	
	return result;
}


namespace {

struct ArchiveReadState {
	ArchiveReadState(ReliSock &sock, filesize_t bytes_to_receive, size_t buf_sz, DCTransferQueue &xfer_q,
		CondorError &err)
	:
		m_sock(sock),
		m_bytes_to_receive(bytes_to_receive),
		m_buf_sz(buf_sz),
		m_xfer_q(xfer_q),
		m_err(err),
		m_buf(new char[buf_sz])
	{}

	ReliSock &m_sock;
	const filesize_t m_bytes_to_receive;
	filesize_t m_bytes_read{0};
	size_t m_buf_sz;
	DCTransferQueue &m_xfer_q;
	CondorError &m_err;
	std::unique_ptr<char[]> m_buf;
};

ssize_t
archive_read(struct archive * /*ar*/, void *client_data, const void **buff) {
	auto state = static_cast<ArchiveReadState*>(client_data);

	if (state->m_bytes_read == state->m_bytes_to_receive) {
		return 0;
	}

	ssize_t iosize = std::min(state->m_buf_sz, static_cast<size_t>(state->m_bytes_to_receive - state->m_bytes_read));

	struct timeval t1, t2;
	condor_gettimestamp(t1);

	int nbytes;

	dprintf(D_FULLDEBUG, "archive_read will try to read %ld bytes.\n", iosize);
	nbytes = state->m_sock.get_bytes(state->m_buf.get(), iosize);
	if (nbytes > 0 && (nbytes + state->m_bytes_read != state->m_bytes_to_receive) && !state->m_sock.end_of_message()) {
		dprintf(D_FULLDEBUG, "archive_read failed to get EOM.\n");
		nbytes = 0;
	}
	state->m_bytes_read += nbytes;

	condor_gettimestamp(t2);
	state->m_xfer_q.AddUsecNetRead(timersub_usec(t2, t1));

	if (nbytes <= 0) {
		state->m_err.pushf("READ", 1, "Failed read of %ld bytes from remote side (retval=%d)",
			iosize, nbytes);
		return -1;
	}

	*buff = state->m_buf.get();

	return nbytes;
}

struct ArchiveWriteState {
	ArchiveWriteState(ReliSock &sock, filesize_t max_bytes, size_t buf_sz, DCTransferQueue &xfer_q,
		CondorError &err)
	:
	m_sock(sock),
	m_max_bytes(max_bytes),
	m_buf_sz(buf_sz),
	m_xfer_q(xfer_q),
	m_err(err),
	m_buf(new char[buf_sz])
	{}

	ReliSock &m_sock;
	const filesize_t m_max_bytes;
	filesize_t m_bytes_written{0};
	filesize_t m_bytes_sent{0};
	size_t m_buf_sz;
	DCTransferQueue &m_xfer_q;
	CondorError &m_err;
	std::unique_ptr<char[]> m_buf;
};

	// Unlike all the other wire protocols for writing files, we have no clue
	// what the final file size is going to be when we start.  Instead of sending
	// a stream of buffers, we first send the size of the next buffer followed by the
	// buffer itself.
ssize_t
archive_write(struct archive * /*ar*/, void *client_data, const void *buf_void, size_t length)
{
	auto buf = static_cast<const char *>(buf_void);
	auto state = static_cast<ArchiveWriteState*>(client_data);
	auto buffer_offset = state->m_bytes_written % state->m_buf_sz;
	auto remaining = state->m_buf_sz - buffer_offset;

	ssize_t bytes_copied = 0;

	//dprintf(D_FULLDEBUG, "archive_write: writing buffer of length %ld; internal buffer offset is %ld.\n", length, buffer_offset);
	while (length)
	{
		if (state->m_max_bytes > 0 && state->m_bytes_written > state->m_max_bytes)
		{
			state->m_err.pushf("XFER", 25, "Resulting archive too large");
			return -1;
		}
		else if (state->m_bytes_written - state->m_bytes_sent == static_cast<ssize_t>(state->m_buf_sz))
		{
			struct timeval t1, t2;
			condor_gettimestamp(t1);

			if (!state->m_sock.put(state->m_buf_sz) || !state->m_sock.end_of_message()) {
				state->m_err.pushf("XFER", 19, "Network error: failed to send buffer size");
				return -1;
			}
			auto nwr = state->m_sock.put_bytes(state->m_buf.get(), state->m_buf_sz);
			if (nwr > 0) {
				//dprintf(D_FULLDEBUG, "Sent archive block of size %d.\n", nwr);
					// Only send EOM now if there's additional data left;
					// the put_archive caller MUST be allowed to call EOM
					// after we exit.
				if (length && !state->m_sock.end_of_message()) {
					state->m_err.pushf("XFER", 17, "Network error: failed to send EOM to remote side");
					return -1;
				}
			}

			condor_gettimestamp(t2);
			state->m_xfer_q.AddUsecNetWrite(timersub_usec(t2, t1));
			state->m_xfer_q.ConsiderSendingReport(t2.tv_sec);

			if (nwr <= 0) {
				state->m_err.pushf("XFER", 18, "Network error: failed to send archive data to remote side");
				return -1;
			}

			state->m_xfer_q.AddBytesSent(nwr);
			state->m_bytes_sent += nwr;
		}
		else
		{
			auto bytes_to_copy = std::min(length, remaining);
			//dprintf(D_FULLDEBUG, "Adding %ld bytes into internal buffer\n", bytes_to_copy);
			memcpy(state->m_buf.get() + buffer_offset, buf, bytes_to_copy);
			buf += bytes_to_copy;
			length -= bytes_to_copy;
			state->m_bytes_written += bytes_to_copy;
			buffer_offset = state->m_bytes_written % state->m_buf_sz;
			remaining = state->m_buf_sz - buffer_offset;
			bytes_copied += bytes_to_copy;
		}
	}

	return bytes_copied;
}


int archive_close(struct archive * /*ar*/, void *client_data)
{
	auto state = static_cast<ArchiveWriteState*>(client_data);

	auto buffer_contents = state->m_bytes_written % state->m_buf_sz;

	if (!state->m_err.empty()) {
		if (!state->m_sock.put(-1)) {
			state->m_err.pushf("XFER", 25, "Network error: failed to send last error message");
			return -1;
		}
		return -1;
	}

	if (buffer_contents)
	{
		struct timeval t1, t2;
		condor_gettimestamp(t1);

		if (!state->m_sock.put(buffer_contents) || !state->m_sock.end_of_message()) {
			state->m_err.pushf("XFER", 21, "Network error: failed to send final buffer size");
			return -1;
		}

		auto nwr = state->m_sock.put_bytes(state->m_buf.get(), buffer_contents);

		condor_gettimestamp(t2);
		state->m_xfer_q.AddUsecNetWrite(timersub_usec(t2, t1));
		state->m_xfer_q.ConsiderSendingReport(t2.tv_sec);

		if (nwr <= 0) {
			state->m_err.pushf("XFER", 22, "Network error: failed to send last archive data to remote side");
			return -1;
		}

		state->m_xfer_q.AddBytesSent(nwr);
	}

		// Note we always have leave a trailing EOM that the caller must perform.
	if ((!state->m_sock.end_of_message() || !state->m_sock.put(0))) {
		state->m_err.pushf("XFER", 19, "Network error: failed to send last archive data to remote side");
		return -1;
	}

	return 0;
}


bool
write_archive_header(struct archive &ar, struct archive_entry &entry, const std::string &full_archive_filename, CondorError &err)
{
	if (ARCHIVE_OK != archive_write_header(&ar, &entry)) {
		dprintf(D_ALWAYS, "Failed to write header for archive file %s: %s (errno=%d)\n",
			full_archive_filename.c_str(), archive_error_string(&ar), archive_errno(&ar));
		if (archive_errno(&ar)) {
			err.pushf("MAKE_ARCHIVE", archive_errno(&ar), "Failed to write header for archive file %s: %s",
				full_archive_filename.c_str(), archive_error_string(&ar));
		} else if (err.empty()) {
			err.pushf("MAKE_ARCHIVE", 1, "Failed to write header for archive file %s (unknown error)",
				full_archive_filename.c_str());
		}
		return false;
	}
	return true;
}


bool
add_directory_to_archive(struct archive &ar, const std::string &dir_path, const std::string &prefix_path, DCTransferQueue &xfer_q, CondorError &err)
{
	Directory contents_iterator(dir_path.c_str());
	const char * f = NULL;
	std::vector<char> working_buffer;
	while( (f = contents_iterator.Next()) ) {
		auto full_archive_filename = prefix_path + DIR_DELIM_CHAR + f;
		auto full_filesystem_path = dir_path + DIR_DELIM_CHAR + f;
		std::unique_ptr<struct archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(), &archive_entry_free);
		archive_entry_set_pathname(entry.get(), full_archive_filename.c_str());
		archive_entry_set_size(entry.get(), contents_iterator.GetFileSize());
		archive_entry_set_perm(entry.get(), contents_iterator.GetMode());
		if (contents_iterator.GetAccessTime()) {
			archive_entry_set_atime(entry.get(), contents_iterator.GetAccessTime(), 0);
		}
		if (contents_iterator.GetModifyTime()) {
			archive_entry_set_mtime(entry.get(), contents_iterator.GetModifyTime(), 0);
		}
		if (contents_iterator.GetCreateTime()) {
			archive_entry_set_ctime(entry.get(), contents_iterator.GetCreateTime(), 0);
		}

		if (contents_iterator.IsRegularFile()) {
			archive_entry_set_filetype(entry.get(), AE_IFREG);
			if (!write_archive_header(ar, *entry.get(), full_archive_filename, err)) {return false;}
			dprintf(D_FULLDEBUG, "Adding regular file %s of size %ld to archive.\n", full_archive_filename.c_str(), contents_iterator.GetFileSize());

			std::unique_ptr<FILE, decltype(&fclose)> fp(safe_fopen_no_create(full_filesystem_path.c_str(), "r"), &fclose);
			if (!fp.get()) {
				dprintf(D_ALWAYS, "Failed to open file %s for archive: %s (errno=%d)\n",
					full_filesystem_path.c_str(), strerror(errno), errno);
				err.pushf("MAKE_ARCHIVE", errno, "Failed to open file %s for archive: %s",
					full_archive_filename.c_str(), strerror(errno));
				return false;
			}

			working_buffer.resize(AES_FILE_BUF_SZ);
			while (true) {
				struct timeval t1,t2;
				condor_gettimestamp(t1);
				ssize_t bytes_read = full_read(fileno(fp.get()), working_buffer.data(), AES_FILE_BUF_SZ);
				condor_gettimestamp(t2);
				xfer_q.AddUsecFileRead(timersub_usec(t2, t1));

				if (bytes_read < 0) {
					dprintf(D_ALWAYS, "Failed to read file %s for archive: %s (errno=%d)\n",
						full_filesystem_path.c_str(), strerror(errno), errno);
					err.pushf("MAKE_ARCHIVE", errno, "Failed to read file %s for archive: %s",
						full_archive_filename.c_str(), strerror(errno));
					return false;
				} else if (bytes_read) {
					//dprintf(D_FULLDEBUG, "Writing %ld bytes to archive.\n", bytes_read);
					if (bytes_read != archive_write_data(&ar, working_buffer.data(), bytes_read)) {
						dprintf(D_ALWAYS, "Failed to write contents of archive file %s: %s (errno=%d)\n",
							full_archive_filename.c_str(), archive_error_string(&ar), archive_errno(&ar));
						if (archive_errno(&ar)) {
							err.pushf("MAKE_ARCHIVE", archive_errno(&ar), "Failed to write contents of archive file %s: %s",
								full_archive_filename.c_str(), archive_error_string(&ar));
						} else if (err.empty()) {
							err.pushf("MAKE_ARCHIVE", 1, "Failed to write contents of archive file %s (unknown error)",
								full_archive_filename.c_str());
						}
						return false;
					}
				} else {
					break;
				}
			}

		} else if (contents_iterator.IsSymlink()) {
			archive_entry_set_filetype(entry.get(), AE_IFLNK);
			ssize_t link_bytes;
			struct stat statbuf;
				// contents_iterator.GetFileSize() returns the size of the target, not of the link.
			if (-1 == lstat(full_filesystem_path.c_str(), &statbuf)) {
				dprintf(D_ALWAYS, "Failed to stat symlink %s for archive: %s (errno=%d)\n",
					full_filesystem_path.c_str(), strerror(errno), errno);
				err.pushf("MAKE_ARCHIVE", errno, "Failed to stat symlink %s for archive: %s",
					full_archive_filename.c_str(), strerror(errno));
				return false;
			}
			working_buffer.resize(statbuf.st_size + 1);
			if ((link_bytes = readlink(full_filesystem_path.c_str(), working_buffer.data(), statbuf.st_size + 1) != statbuf.st_size)) {
				if (link_bytes == -1) {
					dprintf(D_ALWAYS, "Failed to read link for archive file %s: %s (errno=%d)\n",
						full_archive_filename.c_str(), strerror(errno), errno);
					err.pushf("MAKE_ARCHIVE", errno, "Failed to read link for archive file %s: %s",
						full_archive_filename.c_str(), strerror(errno));
					return false;
				}
				dprintf(D_ALWAYS, "Size of link %s changed while creating archive.\n",
					full_archive_filename.c_str());
				err.pushf("MAKE_ARCHIVE", 1, "Size of link %s changed while creating archive.",
					full_archive_filename.c_str());
				return false;
			}
			working_buffer[statbuf.st_size] = '\0';
			dprintf(D_FULLDEBUG, "Adding symlink %s->%s of size %ld to archive.\n", full_archive_filename.c_str(), working_buffer.data(), statbuf.st_size);
			archive_entry_set_symlink(entry.get(), working_buffer.data());

			if (!write_archive_header(ar, *entry.get(), full_archive_filename, err)) {return false;}
		} else if (contents_iterator.IsDirectory()) {
			archive_entry_set_filetype(entry.get(), AE_IFDIR);
			dprintf(D_FULLDEBUG, "Adding directory %s to archive.\n", full_archive_filename.c_str());
			if (!write_archive_header(ar, *entry.get(), full_archive_filename, err)) {return false;}

			if (!add_directory_to_archive(ar, full_filesystem_path, full_archive_filename, xfer_q, err)) {
				return false;
			}
		}
	}
	return true;
}

}


int
ReliSock::get_archive_file(const std::string &filename, filesize_t max_bytes, DCTransferQueue &xfer_q,
	filesize_t &total_bytes_received, CondorError &err)
{
	total_bytes_received = 0;
	std::vector<char> buffer;
	std::unique_ptr<FILE, decltype(&fclose)> fp(safe_fcreate_replace_if_exists(filename.c_str(), "w"), &fclose);
	if (!fp) {
		dprintf(D_ALWAYS, "Failed to open archive file %s for output: %s (errno=%d)\n",
			filename.c_str(), strerror(errno), errno);
		err.pushf("XFER", errno, "Failed to open archive file %s for output: %s",
			filename.c_str(), strerror(errno));
		return GET_FILE_OPEN_FAILED;
	}

	while (true) {
		int blocksize;

		struct timeval t1, t2;
		condor_gettimestamp(t1);

		if ( !get(blocksize) ) {
			dprintf(D_ALWAYS, "ReliSock: get_archive_file: Failed to get block size.\n");
			err.push("XFER", 23, "Failed to get block size.");
			return -1;
		}
		if (blocksize < 0) {
			return -1;
		} else if (blocksize == 0) {
			break;
		}
		if ( !end_of_message() ) {
			dprintf(D_ALWAYS, "ReliSock: get_archive_file: Failed to get block size EOM.\n");
			err.push("XFER", 22, "Failed to get block size EOM.");
			return -1;
		}
		total_bytes_received += blocksize;
		if (max_bytes > 0 && total_bytes_received > max_bytes) {
			total_bytes_received -= blocksize;
			dprintf(D_ALWAYS, "ReliSock: get_archive_file: Remote side sent too many bytes.\n");
			err.pushf("XFER", 24, "Remote side attempted to send too many bytes for %s",
				filename.c_str());
			return -1;
		}

		buffer.resize(blocksize);
		auto received_bytes = get_bytes(buffer.data(), blocksize);
		if ((blocksize != received_bytes) || !end_of_message()) {
			dprintf(D_ALWAYS, "ReliSock: get_archive_file: Failed to get data from network; expected=%d, got=%d.\n",
				blocksize, received_bytes);
			err.pushf("XFER", 25, "Failed to get archive data from network; expected=%d, got=%d",
				blocksize, received_bytes);
			return -1;
		}

		condor_gettimestamp(t2);
		xfer_q.AddUsecNetRead(timersub_usec(t2, t1));

		ssize_t nwr = full_write(fileno(fp.get()), buffer.data(), blocksize);

		condor_gettimestamp(t1);
		xfer_q.AddUsecFileWrite(timersub_usec(t1, t2));
		xfer_q.AddBytesReceived(blocksize);
		xfer_q.ConsiderSendingReport(t1.tv_sec);

		if (nwr < 0) {
			dprintf(D_ALWAYS, "ReliSock: get_archive_file: Failed to write out archive %s: %s (errno=%d)",
				filename.c_str(), strerror(errno), errno);
			err.pushf("XFER", errno, "Failed to write archive %s: %s",
				filename.c_str(), strerror(errno));
			return -1;
		}
	}
	return 0;
}

int
ReliSock::put_archive_file(const std::string &filename, filesize_t max_bytes, DCTransferQueue &xfer_q,
	filesize_t &total_sent, CondorError &err)
{
	filesize_t filesize;
	const size_t buf_sz = AES_FILE_BUF_SZ;

	int rc = 0;
	int fd = -1;

	StatInfo filestat(filename.c_str());
	if (filestat.Error()) {
		auto staterr = filestat.Errno( );
		dprintf(D_ALWAYS, "ReliSock: put_archive_file: stat of %s failed: %s (errno=%d)\n",
			filename.c_str(), strerror(staterr), staterr);
		err.pushf("XFER", 1, "Failed to get status of %s: %s (errno=%d)",
			filename.c_str(), strerror(staterr), staterr);
		rc = PUT_FILE_OPEN_FAILED;
	} else if (filestat.IsDomainSocket() || filestat.IsDirectory()) {
		dprintf(D_ALWAYS, "ReliSock: put_archive_file: Failed because only files are supported.\n" );
		err.pushf("XFER", 2, "%s is not a file; cannot send.", filename.c_str());
		rc = PUT_FILE_OPEN_FAILED;
	} else if (((filesize = filestat.GetFileSize()) > max_bytes) && max_bytes >= 0) {
		dprintf(D_ALWAYS, "ReliSock: put_archive_file: File %s is too large "
			"(max size " FILESIZE_T_FORMAT "; actual size " FILESIZE_T_FORMAT ")\n",
			filename.c_str(), max_bytes, filesize);
		err.pushf("XFER", 3, "File %s is too large (max size " FILESIZE_T_FORMAT
			"; actual size " FILESIZE_T_FORMAT ")\n",
			filename.c_str(), max_bytes, filesize);
		rc = PUT_FILE_MAX_BYTES_EXCEEDED;
	} else if ((fd = open(filename.c_str(), O_RDONLY)) < 0) {
		dprintf(D_ALWAYS, "ReliSock: put_archive_file: Unable to open %s for reading: %s (errno=%d).\n",
			filename.c_str(), strerror(errno), errno);
		err.pushf("XFER", 5, "Unable to open %s for reading: %s (errno=%d).",
			filename.c_str(), strerror(errno), errno);
		rc = PUT_FILE_OPEN_FAILED;
	}
	if (rc) {
		if (!put(-1)) {
			dprintf(D_ALWAYS, "ReliSock: put_archive_file: Failed to send error.\n");
			err.push("XFER", 4, "Failed to send error condition.");
		}
		return -1;
	}
	std::unique_ptr<FILE, decltype(&fclose)> fp(fdopen(fd, "r"), &fclose);

	if (filesize == 0) {
		if (!put(filesize)) {
			err.push("XFER", 16, "Failed to send zero-sized file.");
			return -1;
		}
		return 0;
	}

	if ( !put(filesize) || !end_of_message() ) {
		dprintf(D_ALWAYS, "ReliSock: put_archive_file: Failed to send filesize.\n");
		err.push("XFER", 4, "Failed to send file size.");
		return -1;
	}

	dprintf(D_FULLDEBUG, "put_archive_file: sending " FILESIZE_T_FORMAT " bytes\n", filesize);

	std::unique_ptr<char[]> buf(new char[buf_sz]);

	auto bytes_to_send = filesize;
	size_t bytes_sent = 0;
	ssize_t nrd = 0, nwr = 0;
	int saved_errno = 0;
	while (bytes_to_send - bytes_sent > 0)
	{
		size_t iosize = std::min(buf_sz, bytes_to_send - bytes_sent);

		struct timeval t1, t2;
		condor_gettimestamp(t1);
		nrd = full_read(fd, buf.get(), iosize);
		saved_errno = errno;
		condor_gettimestamp(t2);

		xfer_q.AddUsecFileRead(timersub_usec(t2, t1));

		if (nrd < 0) {
			saved_errno = errno;
			break;
		}

		auto nwr = put_bytes(buf.get(), nrd);
		if (nwr > 0 && (static_cast<filesize_t>(nwr + bytes_sent) != bytes_to_send) && !end_of_message()) {
			nwr = -1;

			saved_errno = EIO;
			break;
		} else if (nwr < 0) {
			saved_errno = errno;
			break;
		}

		condor_gettimestamp(t1);
		xfer_q.AddUsecNetWrite(timersub_usec(t1, t2));
		xfer_q.AddBytesSent(nwr);
		xfer_q.ConsiderSendingReport(t1.tv_sec);
		bytes_sent += nwr;
	}

	dprintf(D_FULLDEBUG,
			"ReliSock: put_archive_file: sent " FILESIZE_T_FORMAT " bytes\n", bytes_sent);

	if (nrd < 0) {
		dprintf(D_ALWAYS, "ReliSock: put_archive_file: error when reading %s: %s (errno=%d)\n",
			filename.c_str(), strerror(saved_errno), saved_errno);
		err.pushf("XFER", saved_errno, "Error when reading %s: %s",
			filename.c_str(), strerror(saved_errno));
		return -1;
	} else if (nwr < 0) {
		dprintf(D_ALWAYS, "ReliSock: put_archive_file: error when writing to socket: %s (errno=%d)",
			strerror(saved_errno), saved_errno);
		err.pushf("XFER", saved_errno, "Error when sending %s: %s",
			filename.c_str(), strerror(saved_errno));
	}

	total_sent = filesize;
	return 0;
}


int
ReliSock::get_archive(const std::string &filename, filesize_t max_bytes, DCTransferQueue &xfer_q, filesize_t &bytes, CondorError &err)
{
	filesize_t filesize, bytes_to_receive;
	size_t buf_sz = AES_FILE_BUF_SZ;

	if (!get(filesize) || !end_of_message()) {
		dprintf(D_ALWAYS, "Failed to receive filesize in ReliSock::get_archive\n");
		err.push("XFER", 1, "Failed to receive filesize of archive.");
		return -1;
	} else if (filesize < 0) {
		dprintf(D_ALWAYS, "Remote size indicated failure to start archive %s transfer.\n",
			filename.c_str());
		err.push("XFER", 16, "Remote size indicated failure to start archive transfer.");
		return -1;
	}
	bytes_to_receive = filesize;
	if (max_bytes > 0 && bytes_to_receive > max_bytes) {
		err.pushf("XFER", 2, "Expecting archive of size " FILESIZE_T_FORMAT ", which is"
		         " greater than max file size of " FILESIZE_T_FORMAT " bytes",
		         bytes_to_receive, max_bytes);
		return -1;
	}

	std::unique_ptr<char[]> buf(new char[buf_sz]);

	// Log what's going on
	dprintf(D_FULLDEBUG,
		"get_archive: Receiving archive of " FILESIZE_T_FORMAT " bytes\n",
		 bytes_to_receive);

	// Now we hand over to libarchive
#ifdef HAVE_LIBARCHIVE
	ArchiveReadState state(*this, bytes_to_receive, buf_sz, xfer_q, err);
	size_t last_bytes_received_report = 0;
	struct archive *ar = archive_read_new();
	std::unique_ptr<struct archive, decltype(&archive_read_free)> ar_helper(ar, &archive_read_free);
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if (ARCHIVE_OK != archive_read_open(ar, reinterpret_cast<void*>(&state), nullptr, archive_read, nullptr)) {
		return -1;
	}
	int rc = 0;
	struct archive_entry *entry;
	while (true) {
		rc = archive_read_next_header(ar, &entry);
		if (rc == ARCHIVE_RETRY) {continue;}
		else if (rc == ARCHIVE_FATAL || rc == ARCHIVE_EOF) {break;}

		auto name = archive_entry_pathname(entry);
		if (!name) {
			err.pushf("XFER", 3, "Received archive entry with no valid pathname in %s", filename.c_str());
			return -1;
		}
		while (*name && *name == '/') {name++;}
		if (!*name) {
			err.pushf("XFER", 4, "Received an archive with an empty filename in %s", filename.c_str());
			return -1;
		}

		int mode = archive_entry_mode(entry);

		struct utimbuf times;
		times.actime = archive_entry_atime(entry);
		times.modtime = archive_entry_mtime(entry);

		int filetype = archive_entry_filetype(entry);
		if (filetype == AE_IFREG) {
			dprintf(D_FULLDEBUG, "Unpacking regular file %s from archive %s.\n", name, filename.c_str());

			auto filesize = archive_entry_size(entry);
			if (filesize < 0) {
				err.pushf("XFER", 6, "File %s in archive %s has invalid size (%ld)",
					name, filename.c_str(), filesize);
				return -1;
			}

			if (!make_parents_if_needed(name, 0755)) {
				err.pushf("XFER", 14, "Failed to create parent directory of %s"
					" from archive %s: %s (errno=%d)",
					name, filename.c_str(), strerror(errno), errno);
				return -1;
			}

			auto fd = open(name, O_CREAT|O_TRUNC|O_WRONLY, mode);
			if (fd == -1) {
				err.pushf("XFER", 5, "Failed to open file %s from archive %s for writing: %s (errno=%d)",
					name, filename.c_str(), strerror(errno), errno);
				return -1;
			}
			std::unique_ptr<FILE, decltype(&fclose)> fp(fdopen(fd, "w"), &fclose);

			ssize_t bytes_read = 0;
			while (filesize - bytes_read > 0) {
				ssize_t bytes = archive_read_data(ar, buf.get(), buf_sz);
				if (bytes < 0) {
					err.pushf("XFER", 7, "File %s from archive %s data read failure.\n",
						name, filename.c_str());
					return -1;
				}
				struct timeval t1, t2;
				condor_gettimestamp(t1);
				auto bytes_written = full_write(fd, buf.get(), bytes);
				condor_gettimestamp(t2);
				xfer_q.AddUsecFileWrite(timersub_usec(t1, t2));

				if (bytes_written < 0) {
					err.pushf("XFER", 8, "File %s from archive %s data write error: %s (errno=%d)",
						name, filename.c_str(), strerror(errno), errno);
					return -1;
				}
				xfer_q.AddBytesReceived(state.m_bytes_read - last_bytes_received_report);
				last_bytes_received_report = state.m_bytes_read;
				xfer_q.ConsiderSendingReport(t2.tv_sec);
				bytes_read += bytes;
			}

		} else if (filetype == AE_IFLNK) {
			dprintf(D_FULLDEBUG, "Unpacking soft link %s.\n", name);

			if (!make_parents_if_needed(name, 0755)) {
				err.pushf("XFER", 14, "Failed to create parent directory of symlink %s"
					" from archive %s: %s (errno=%d)",
					name, filename.c_str(), strerror(errno), errno);
				return -1;
			}

			auto link_value =  archive_entry_symlink(entry);
			if (!link_value) {
				err.pushf("XFER", 13, "Symlink %s from archive %s has no target",
					name, filename.c_str());
				return -1;
			}
			if (-1 == symlink(link_value, name)) {
				err.pushf("XFER", 16, "Failed to set symlink %s->%s from archive"
					" %s: %s (errno=%d)", link_value, name, filename.c_str(),
					strerror(errno), errno);
				return -1;
			}

		} else if (filetype == AE_IFDIR) {
			dprintf(D_FULLDEBUG, "Unpacking directory %s.\n", name);
			if (!mkdir_and_parents_if_needed(name, mode)) {
				err.pushf("XFER", 15, "Failed to create directory %s from archive %s:"
					" %s (errno=%d)", name, filename.c_str(), strerror(errno),
					errno);
				return -1;
			}
		} else if (filetype == AE_IFSOCK) {
			dprintf(D_FULLDEBUG, "Unsupported file type 'socket' for %s.\n", name);
			continue;
		} else if (filetype == AE_IFCHR) {
			dprintf(D_FULLDEBUG, "Unsupported file type 'character device' for %s.\n", name);
			continue;
		} else if (filetype == AE_IFBLK) {
			dprintf(D_FULLDEBUG, "Unsupported file type 'block device' for %s.\n", name);
			continue;
		} else if (filetype == AE_IFIFO) {
			dprintf(D_FULLDEBUG, "Unsupported file type 'fifo' for %s.\n", name);
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Unknown file type %d for %s.\n", filetype, name);
			continue;
		}

		utime(name, &times);
	}

	xfer_q.AddBytesReceived(state.m_bytes_read - last_bytes_received_report);
	struct timeval t2;
	condor_gettimestamp(t2);
	xfer_q.ConsiderSendingReport(t2.tv_sec);

	if (rc == ARCHIVE_FATAL) {
		err.pushf("XFER", 12, "Fatal error when receiving archive %s: %s",
			filename.c_str(), archive_error_string(ar));
		return -1;
	}

	bytes = state.m_bytes_read;
	return 0;
#else
	dprintf(D_ALWAYS, "get_archive: archives are not supported\n");
	err.pushf("XFER", 100, "Remote side tried to send an archive (%s) file but this is not supported", filename.c_str());
	return -1;
#endif
}


int
ReliSock::put_archive(const std::string &filename, filesize_t max_bytes, DCTransferQueue &xfer_q, filesize_t &bytes, CondorError &err)
{
	size_t buf_sz = AES_FILE_BUF_SZ;

	std::unique_ptr<char[]> buf(new char[buf_sz]);

	// Log what's going on
	dprintf(D_FULLDEBUG, "put_archive: Streaming archive from %s\n", filename.c_str());

	// Now we hand over to libarchive
#ifdef HAVE_LIBARCHIVE
	ArchiveWriteState state(*this, max_bytes, buf_sz, xfer_q, err);
	struct archive *ar = archive_write_new();
	std::unique_ptr<struct archive, decltype(&archive_write_free)> ar_helper(ar, &archive_write_free);
	archive_write_add_filter_gzip(ar);
	archive_write_set_format_gnutar(ar);
	if (ARCHIVE_OK != archive_write_open(ar, reinterpret_cast<void*>(&state), nullptr, archive_write, archive_close)) {
		return -1;
	}

	int rc = add_directory_to_archive(*ar, filename, condor_basename(filename.c_str()), xfer_q, err) ? 0 : -1;
	if (rc == 0) {
		bytes = state.m_bytes_written;
	}

	return rc;
#else
	dprintf(D_ALWAYS, "put_archive: archives are not supported\n");
	err.pushf("XFER", 101, "Remote side tried to send an archive (%s) file but this is not supported", filename.c_str());
	return -1;
#endif
}


MSC_DISABLE_WARNING(6262) // function uses 64k of stack
int
ReliSock::get_file( filesize_t *size, int fd,
					bool flush_buffers, bool append, filesize_t max_bytes,
					DCTransferQueue *xfer_q)
{
	filesize_t filesize, bytes_to_receive;
	unsigned int eom_num;
	filesize_t total = 0;
	int retval = 0;
	int saved_errno = 0;
	bool buffered = get_encryption() && get_crypto_state()->m_keyInfo.getProtocol() == CONDOR_AESGCM;
	size_t buf_sz = OLD_FILE_BUF_SZ;

		// NOTE: the caller may pass fd=GET_FILE_NULL_FD, in which
		// case we just read but do not write the data.

	// Read the filesize from the other end of the wire
	// If we're operating in buffered mode, also read the buffer size
	// (each buffer-sized chunk will be sent in a seperate CEDAR message).
	if ( !get(filesize) || !(buffered ? get(buf_sz) : 1) || !end_of_message() ) {
		dprintf(D_ALWAYS, 
				"Failed to receive filesize in ReliSock::get_file\n");
		return -1;
	}
	bytes_to_receive = filesize;
	if ( append ) {
		lseek( fd, 0, SEEK_END );
	}

	std::unique_ptr<char[]> buf(new char[buf_sz]);

	// Log what's going on
	dprintf( D_FULLDEBUG,
			 "get_file: Receiving " FILESIZE_T_FORMAT " bytes\n",
			 bytes_to_receive );

		/*
		  the code used to check for filesize == -1 here, but that's
		  totally wrong.  we're storing the size as an unsigned int,
		  so we can't check it against a negative value.  furthermore,
		  ReliSock::put_file() never puts a -1 on the wire for the
		  size.  that's legacy code from the pseudo_put_file_stream()
		  RSC in the syscall library.  this code isn't like that.
		*/

	// Now, read it all in & save it
	while( total < bytes_to_receive ) {
		struct timeval t1,t2;
		if( xfer_q ) {
			condor_gettimestamp(t1);
		}

		int	iosize =
			(int) MIN( (filesize_t) buf_sz, bytes_to_receive - total );
		int	nbytes;
		if( buffered ) {
			nbytes = get_bytes( buf.get(), iosize );
			if( nbytes > 0 && !end_of_message() ) {
				nbytes = 0;
			}
		} else {
			nbytes = get_bytes_nobuffer( buf.get(), iosize, 0 );
		}

		if( xfer_q ) {
			condor_gettimestamp(t2);
			xfer_q->AddUsecNetRead(timersub_usec(t2, t1));
		}

		if ( nbytes <= 0 ) {
			break;
		}

		if( fd == GET_FILE_NULL_FD ) {
				// Do not write the data, because we are just
				// fast-forwarding and throwing it away, due to errors
				// already encountered.
			total += nbytes;
			continue;
		}

		int rval;
		int written;
		for( written=0; written<nbytes; ) {
			rval = ::write( fd, &buf[written], (nbytes-written) );
			if( rval < 0 ) {
				saved_errno = errno;
				dprintf( D_ALWAYS,
						 "ReliSock::get_file: write() returned %d: %s "
						 "(errno=%d)\n", rval, strerror(errno), errno );


					// Continue reading data, but throw it all away.
					// In this way, we keep the wire protocol in a
					// well defined state.
				fd = GET_FILE_NULL_FD;
				retval = GET_FILE_WRITE_FAILED;
				written = nbytes;
				break;
			} else if( rval == 0 ) {
					/*
					  write() shouldn't really return 0 at all.
					  apparently it can do so if we asked it to write
					  0 bytes (which we're not going to do) or if the
					  file is closed (which we're also not going to
					  do).  so, for now, if we see it, we want to just
					  break out of this loop.  in the future, we might
					  do more fancy stuff to handle this case, but
					  we're probably never going to see this anyway.
					*/
				dprintf( D_ALWAYS,
						 "ReliSock::get_file: write() returned 0: "
						 "wrote %d out of %d bytes (errno=%d %s)\n",
						 written, nbytes, errno, strerror(errno) );
				break;
			} else {
				written += rval;
			}
		}
		if( xfer_q ) {
			condor_gettimestamp(t1);
				// reuse t2 above as start time for file write
			xfer_q->AddUsecFileWrite(timersub_usec(t1, t2));
			xfer_q->AddBytesReceived(written);
			xfer_q->ConsiderSendingReport(t1.tv_sec);
		}

		total += written;
		if( max_bytes >= 0 && total > max_bytes ) {

				// Since breaking off here leaves the protocol in an
				// undefined state, further communication about what
				// went wrong is not possible.  We do not want to
				// consume all remaining bytes to keep our peer happy,
				// because max_bytes may have been specified to limit
				// network usage.  Therefore, it is best to avoid ever
				// getting here.  For example, in FileTransfer, the
				// sender is expected to be nice and never send more
				// than the receiver's limit; we only get here if the
				// sender is behaving unexpectedly.

			dprintf( D_ALWAYS, "get_file: aborting after downloading %ld of %ld bytes, because max transfer size is exceeded.\n",
					 (long int)total,
					 (long int)bytes_to_receive);
			return GET_FILE_MAX_BYTES_EXCEEDED;
		}
	}

	// Our caller may treat get_file() as the end of a CEDAR message
	// and call end_of_message immediately afterwards. This call will
	// keep that from failing.
	if (buffered && !prepare_for_nobuffering(stream_decode)) {
		dprintf( D_ALWAYS, "get_file: prepare_for_nobuffering() failed!\n" );
		return -1;
	}

	if ( filesize == 0 ) {
		if ( !get(eom_num) || eom_num != PUT_FILE_EOM_NUM ) {
			dprintf( D_ALWAYS, "get_file: Zero-length file check failed!\n" );
			return -1;
		}			
	}

	if (flush_buffers && fd != GET_FILE_NULL_FD ) {
		if (condor_fdatasync(fd) < 0) {
			dprintf(D_ALWAYS, "get_file(): ERROR on fsync: %d\n", errno);
			return -1;
		}
	}

	if( fd == GET_FILE_NULL_FD ) {
		dprintf( D_ALWAYS,
				 "get_file(): consumed " FILESIZE_T_FORMAT
				 " bytes of file transmission\n",
				 total );
	}
	else {
		dprintf( D_FULLDEBUG,
				 "get_file: wrote " FILESIZE_T_FORMAT " bytes to file\n",
				 total );
	}

	if ( total < filesize ) {
		dprintf( D_ALWAYS,
				 "get_file(): ERROR: received " FILESIZE_T_FORMAT " bytes, "
				 "expected " FILESIZE_T_FORMAT "!\n",
				 total, filesize);
		return -1;
	}

	*size = total;
	errno = saved_errno;
	return retval;
}
MSC_RESTORE_WARNING(6262) // function uses 64k of stack

int
ReliSock::put_empty_file( filesize_t *size )
{
	bool buffered = get_encryption() && get_crypto_state()->m_keyInfo.getProtocol() == CONDOR_AESGCM;
	*size = 0;
	// the put(1) here is required because the other size is expecting us
	// to send the size of messages we are going to use.  however, we're
	// send zero bytes total so we just need to send any int at all, which
	// then gets ignored on the other side.
	if(!put(*size) || !(buffered ? put(1) : 1) || !end_of_message()) {
		dprintf(D_ALWAYS,"ReliSock: put_file: failed to send dummy file size\n");
		return -1;
	}
	put(PUT_FILE_EOM_NUM); //end the zero-length file
	return 0;
}

int
ReliSock::put_file( filesize_t *size, const char *source, filesize_t offset, filesize_t max_bytes, DCTransferQueue *xfer_q)
{
	int fd;
	int result;

	// Open the file, handle failure
	if (allow_shadow_access(source)) {
		errno = 0;
		fd = safe_open_wrapper_follow(source, O_RDONLY | O_LARGEFILE | _O_BINARY | _O_SEQUENTIAL, 0);
	}
	else {
		fd = -1;
		errno = EACCES;
	}

	if ( fd < 0 )
	{
		dprintf(D_ALWAYS,
				"ReliSock: put_file: Failed to open file %s, errno = %d.\n",
				source, errno);
			// Give the receiver an empty file so that this message is
			// complete.  The receiver _must_ detect failure through
			// some additional communication that is not part of
			// the put_file() message.

		int rc = put_empty_file( size );
		if(rc < 0) {
			return rc;
		}

		return PUT_FILE_OPEN_FAILED;
	}

	dprintf( D_FULLDEBUG,
			 "put_file: going to send from filename %s\n", source);

	result = put_file( size, fd, offset, max_bytes, xfer_q );

	if (::close(fd) < 0) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: close failed, errno = %d (%s)\n",
				errno, strerror(errno));
		return -1;
	}

	return result;
}

MSC_DISABLE_WARNING(6262) // function uses 64k of stack
int
ReliSock::put_file( filesize_t *size, int fd, filesize_t offset, filesize_t max_bytes, DCTransferQueue *xfer_q )
{
	filesize_t	filesize;
	filesize_t	total = 0;
	bool buffered = get_encryption() && get_crypto_state()->m_keyInfo.getProtocol() == CONDOR_AESGCM;
	const size_t buf_sz = buffered ? AES_FILE_BUF_SZ : OLD_FILE_BUF_SZ;

	StatInfo filestat( fd );
	if ( filestat.Error() ) {
		int		staterr = filestat.Errno( );
		dprintf(D_ALWAYS, "ReliSock: put_file: StatBuf failed: %d %s\n",
				staterr, strerror( staterr ) );
		return -1;
	}

	if ( filestat.IsDirectory() ) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: Failed because directories are not supported.\n" );
			// Give the receiver an empty file so that this message is
			// complete.  The receiver _must_ detect failure through
			// some additional communication that is not part of
			// the put_file() message.

		int rc = put_empty_file( size );
		if(rc < 0) {
			return rc;
		}

#ifdef EISDIR
		errno = EISDIR;
#else
		errno = EINVAL;
#endif
		return PUT_FILE_OPEN_FAILED;
	}

	filesize = filestat.GetFileSize( );
	dprintf( D_FULLDEBUG,
			 "put_file: Found file size " FILESIZE_T_FORMAT "\n",
			 filesize );
	
	if ( offset > filesize ) {
		dprintf( D_ALWAYS,
				 "ReliSock::put_file: offset " FILESIZE_T_FORMAT
				 " is larger than file " FILESIZE_T_FORMAT "!\n",
				 offset, filesize );
	}
	filesize_t	bytes_to_send = filesize - offset;
	bool max_bytes_exceeded = false;
	if( max_bytes >= 0 && bytes_to_send > max_bytes ) {
		bytes_to_send = max_bytes;
		max_bytes_exceeded = true;
	}

	// Send the file size to the receiver
	// If we're operating in buffered mode, also send the buffer size
	// (each buffer-sized chunk will be sent in a seperate CEDAR message).
	if ( !put(bytes_to_send) || !(buffered ? put(buf_sz) : 1) || !end_of_message() ) {
		dprintf(D_ALWAYS, "ReliSock: put_file: Failed to send filesize.\n");
		return -1;
	}

	if ( offset ) {
		lseek( fd, offset, SEEK_SET );
	}

	// Log what's going on
	dprintf(D_FULLDEBUG,
			"put_file: sending " FILESIZE_T_FORMAT " bytes\n", bytes_to_send );

	// If the file has a non-zero size, send it
	if ( bytes_to_send > 0 ) {

#if defined(WIN32)
		// On Win32, if we don't need encryption, use the super-efficient Win32
		// TransmitFile system call. Also, TransmitFile does not support
		// file sizes over 2GB, so we avoid that case as well.
		if (  (!get_encryption()) &&
			  (0 == offset) &&
			  (bytes_to_send < INT_MAX)  ) {

			// First drain outgoing buffers
			if ( !prepare_for_nobuffering(stream_encode) ) {
				dprintf(D_ALWAYS,
						"ReliSock: put_file: failed to drain buffers!\n");
				return -1;
			}

			struct timeval t1;
			condor_gettimestamp(t1);

			// Now transmit file using special optimized Winsock call
			bool transmit_success = TransmitFile(_sock,(HANDLE)_get_osfhandle(fd),bytes_to_send,0,NULL,NULL,0) != FALSE;

			if( xfer_q ) {
				struct timeval t2;
				condor_gettimestamp(t2);
					// We don't know how much of the time was spent reading
					// from disk vs. writing to the network, so we just report
					// it all as network i/o time.
				xfer_q->AddUsecNetWrite(timersub_usec(t2, t1));
			}

			if( !transmit_success ) {
				dprintf(D_ALWAYS,
						"ReliSock: put_file: TransmitFile() failed, errno=%d\n",
						GetLastError() );
				return -1;
			} else {
				// Note that it's been sent, so that we don't try to below
				total = bytes_to_send;
				if( xfer_q ) {
					xfer_q->AddBytesSent(bytes_to_send);
					xfer_q->ConsiderSendingReport();
				}
			}
		}
#endif

		std::unique_ptr<char[]> buf(new char[buf_sz]);
		int nbytes, nrd;

		// On Unix, always send the file using put_bytes_nobuffer().
		// Note that on Win32, we use this method as well if encryption 
		// is required.
		while (total < bytes_to_send) {
			struct timeval t1;
			struct timeval t2;
			if( xfer_q ) {
				condor_gettimestamp(t1);
			}

			// Be very careful about where the cast to size_t happens; see gt#4150
			nrd = ::read(fd, buf.get(), (size_t)((bytes_to_send-total) < (int)buf_sz ? bytes_to_send-total : buf_sz));

			if( xfer_q ) {
				condor_gettimestamp(t2);
				xfer_q->AddUsecFileRead(timersub_usec(t2, t1));
			}

			if( nrd <= 0) {
				break;
			}
			if( buffered ) {
				nbytes = put_bytes(buf.get(), nrd);
				if( nbytes > 0 && !end_of_message() ) {
					nbytes = 0;
				}
			} else {
				nbytes = put_bytes_nobuffer(buf.get(), nrd, 0);
			}
			if (nbytes < nrd) {
					// put_bytes_nobuffer() does the appropriate
					// looping for us already, the only way this could
					// return less than we asked for is if it returned
					// -1 on failure.
				ASSERT( nbytes <= 0 );
				dprintf( D_ALWAYS, "ReliSock::put_file: failed to put %d "
						 "bytes (put_bytes_nobuffer() returned %d)\n",
						 nrd, nbytes );
				return -1;
			}
			if( xfer_q ) {
					// reuse t2 from above to mark the start of the
					// network op and t1 to mark the end
				condor_gettimestamp(t1);
				xfer_q->AddUsecNetWrite(timersub_usec(t1, t2));
				xfer_q->AddBytesSent(nbytes);
				xfer_q->ConsiderSendingReport(t1.tv_sec);
			}
			total += nbytes;
		}
	
	} // end of if filesize > 0

	// Our caller may treat put_file() as the end of a CEDAR message
	// and call end_of_message immediately afterwards. This call will
	// keep that from failing.
	if (buffered && !prepare_for_nobuffering(stream_encode)) {
		dprintf( D_ALWAYS, "put_file: prepare_for_nobuffering() failed!\n" );
		return -1;
	}

	if ( bytes_to_send == 0 ) {
		put(PUT_FILE_EOM_NUM);
	}

	dprintf(D_FULLDEBUG,
			"ReliSock: put_file: sent " FILESIZE_T_FORMAT " bytes\n", total);

	if (total < bytes_to_send) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: only sent " FILESIZE_T_FORMAT 
				" bytes out of " FILESIZE_T_FORMAT "\n",
				total, filesize);
		return -1;
	}

	if ( max_bytes_exceeded ) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: only sent " FILESIZE_T_FORMAT 
				" bytes out of " FILESIZE_T_FORMAT
				" because maximum upload bytes was exceeded.\n",
				total, filesize);
		*size = bytes_to_send;
			// Nothing we have done has told the receiver that we hit
			// the max_bytes limit.  The receiver _must_ detect
			// failure through some additional communication that is
			// not part of the put_file() message.
		return PUT_FILE_MAX_BYTES_EXCEEDED;
	}

	*size = filesize;
	return 0;
}
MSC_RESTORE_WARNING(6262) // function uses 64k of stack

int
ReliSock::get_file_with_permissions( filesize_t *size, 
									 const char *destination,
									 bool flush_buffers,
									 filesize_t max_bytes,
									 DCTransferQueue *xfer_q)
{
	int result;
	condor_mode_t file_mode;

	// Read the permissions
	this->decode();
	if ( this->code( file_mode ) == FALSE ||
		 this->end_of_message() == FALSE ) {
		dprintf( D_ALWAYS, "ReliSock::get_file_with_permissions(): "
				 "Failed to read permissions from peer\n" );
		return -1;
	}

	result = get_file( size, destination, flush_buffers, false, max_bytes, xfer_q );

	if ( result < 0 ) {
		return result;
	}

	if( destination && !strcmp(destination,NULL_FILE) ) {
		return result;
	}

		// If the other side told us to ignore its permissions, then we're
		// done.
	if ( file_mode == NULL_FILE_PERMISSIONS ) {
		dprintf( D_FULLDEBUG, "ReliSock::get_file_with_permissions(): "
				 "received null permissions from peer, not setting\n" );
		return result;
	}

		// We don't know how unix permissions translate to windows, so
		// ignore whatever permissions we received if we're on windows.
#ifndef WIN32
	dprintf( D_FULLDEBUG, "ReliSock::get_file_with_permissions(): "
			 "going to set permissions %o\n", file_mode );

	// chmod the file
	errno = 0;
	result = ::chmod( destination, (mode_t)file_mode );
	if ( result < 0 ) {
		dprintf( D_ALWAYS, "ReliSock::get_file_with_permissions(): "
				 "Failed to chmod file '%s': %s (errno: %d)\n",
				 destination, strerror(errno), errno );
		return -1;
	}
#endif

	return result;
}


int
ReliSock::put_file_with_permissions( filesize_t *size, const char *source, filesize_t max_bytes, DCTransferQueue *xfer_q )
{
	int result;
	condor_mode_t file_mode;

#ifndef WIN32
	// Stat the file
	StatInfo stat_info( source );

	if ( stat_info.Error() ) {
		dprintf( D_ALWAYS, "ReliSock::put_file_with_permissions(): "
				 "Failed to stat file '%s': %s (errno: %d, si_error: %d)\n",
				 source, strerror(stat_info.Errno()), stat_info.Errno(),
				 stat_info.Error() );

		// Now send an empty file in order to recover sanity on this
		// stream.
		file_mode = NULL_FILE_PERMISSIONS;
		this->encode();
		if( this->code( file_mode) == FALSE ||
			this->end_of_message() == FALSE ) {
			dprintf( D_ALWAYS, "ReliSock::put_file_with_permissions(): "
			         "Failed to send dummy permissions\n" );
			return -1;
		}
		int rc = put_empty_file( size );
		if(rc < 0) {
			return rc;
		}
		return PUT_FILE_OPEN_FAILED;
	}
	file_mode = (condor_mode_t)stat_info.GetMode();
#else
		// We don't know what unix permissions a windows file should have,
		// so tell the other side to ignore permissions from us (act like
		// get/put_file() ).
	file_mode = NULL_FILE_PERMISSIONS;
#endif

	dprintf( D_FULLDEBUG, "ReliSock::put_file_with_permissions(): "
			 "going to send permissions %o\n", file_mode );

	// Send the permissions
	this->encode();
	if ( this->code( file_mode ) == FALSE ||
		 this->end_of_message() == FALSE ) {
		dprintf( D_ALWAYS, "ReliSock::put_file_with_permissions(): "
				 "Failed to send permissions\n" );
		return -1;
	}

	result = put_file( size, source, 0, max_bytes, xfer_q );

	return result;
}

ReliSock::x509_delegation_result
ReliSock::get_x509_delegation( const char *destination,
                               bool flush_buffers, void **state_ptr )
{
	void *st;
	int in_encode_mode;

		// store if we are in encode or decode mode
	in_encode_mode = is_encode();

	if ( !prepare_for_nobuffering( stream_unknown ) ||
		 !end_of_message() ) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): failed to "
				 "flush buffers\n" );
		return delegation_error;
	}

	int rc =  x509_receive_delegation( destination, relisock_gsi_get, (void *) this,
	                                                relisock_gsi_put, (void *) this,
	                                                &st );
	if (rc == -1) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): "
				 "delegation failed: %s\n", x509_error_string() );
		return delegation_error;
	}
		// NOTE: if we provide a state pointer, x509_receive_delegation *must* either fail or continue.
	else if (rc == 0) {
		dprintf(D_ALWAYS, "Programmer error: x509_receive_delegation completed unexpectedy.\n");
		return delegation_error;
	}

		// restore stream mode (either encode or decode)
	if ( in_encode_mode && is_decode() ) {
		encode();
	} else if ( !in_encode_mode && is_encode() ) { 
		decode();
	}

	if (state_ptr) {
		*state_ptr = st;
		return delegation_continue;
	}

	return get_x509_delegation_finish( destination, flush_buffers, st );
}

ReliSock::x509_delegation_result
ReliSock::get_x509_delegation_finish( const char *destination, bool flush_buffers, void *state_ptr )
{
                // store if we are in encode or decode mode
        int in_encode_mode = is_encode();

        if ( x509_receive_delegation_finish( relisock_gsi_get, (void *) this, state_ptr ) != 0 ) {
                dprintf( D_ALWAYS, "ReliSock::get_x509_delegation_finish(): "
                                 "delegation failed to complete: %s\n", x509_error_string() );
                return delegation_error;
        }

	if ( flush_buffers ) {
		int rc = 0;
		int fd = safe_open_wrapper_follow( destination, O_WRONLY, 0 );
		if ( fd < 0 ) {
			rc = fd;
		} else {
			rc = condor_fdatasync( fd, destination );
			::close( fd );
		}
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): open/fsync "
					 "failed, errno=%d (%s)\n", errno,
					 strerror( errno ) );
		}
	}

		// restore stream mode (either encode or decode)
	if ( in_encode_mode && is_decode() ) {
		encode();
	} else if ( !in_encode_mode && is_encode() ) {
		decode();
	}
	if ( !prepare_for_nobuffering( stream_unknown ) ) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): failed to "
			"flush buffers afterwards\n" );
		return delegation_error;
	}

	return delegation_ok;
}

int
ReliSock::put_x509_delegation( filesize_t *size, const char *source, time_t expiration_time, time_t *result_expiration_time )
{
	int in_encode_mode;

		// store if we are in encode or decode mode
	in_encode_mode = is_encode();

	if ( !prepare_for_nobuffering( stream_unknown ) ||
		 !end_of_message() ) {
		dprintf( D_ALWAYS, "ReliSock::put_x509_delegation(): failed to "
				 "flush buffers\n" );
		return -1;
	}

	if ( x509_send_delegation( source, expiration_time, result_expiration_time, relisock_gsi_get, (void *) this,
							   relisock_gsi_put, (void *) this ) != 0 ) {
		dprintf( D_ALWAYS, "ReliSock::put_x509_delegation(): delegation "
				 "failed: %s\n", x509_error_string() );
		return -1;
	}

		// restore stream mode (either encode or decode)
	if ( in_encode_mode && is_decode() ) {
		encode();
	} else if ( !in_encode_mode && is_encode() ) { 
		decode();
	}
	if ( !prepare_for_nobuffering( stream_unknown ) ) {
		dprintf( D_ALWAYS, "ReliSock::put_x509_delegation(): failed to "
				 "flush buffers afterwards\n" );
		return -1;
	}

		// We should figure out how many bytes were sent
	*size = 0;

	return 0;
}

// These variables hold the size of the last data block handled by each
// respective function. They are part of a hacky workaround for a GSI bug.
size_t relisock_gsi_get_last_size = 0;
size_t relisock_gsi_put_last_size = 0;

int relisock_gsi_get(void *arg, void **bufp, size_t *sizep)
{
    /* globus code which calls this function expects 0/-1 return vals */
    
    ReliSock * sock = (ReliSock*) arg;
    size_t stat;
    
    sock->decode();
    
    //read size of data to read
    stat = sock->code( *sizep );
	if ( stat == FALSE ) {
		*sizep = 0;
	}

	if( *sizep == 0 ) {
			// We avoid calling malloc(0) here, because the zero-length
			// buffer is not being freed by globus.
		*bufp = NULL;
	}
	else {
		*bufp = malloc( *sizep );
		if ( !*bufp ) {
			dprintf( D_ALWAYS, "malloc failure relisock_gsi_get\n" );
			stat = FALSE;
		}

			//if successfully read size and malloced, read data
		if ( stat ) {
			stat = sock->code_bytes( *bufp, *sizep );
		}
	}
    
    sock->end_of_message();
    
    //check to ensure comms were successful
    if ( stat == FALSE ) {
        dprintf( D_ALWAYS, "relisock_gsi_get (read from socket) failure\n" );
        *sizep = 0;
        free( *bufp );
        *bufp = NULL;
        relisock_gsi_get_last_size = 0;
        return -1;
    }
    relisock_gsi_get_last_size = *sizep;
    return 0;
}

int relisock_gsi_put(void *arg,  void *buf, size_t size)
{
    //param is just a ReliSock*
    ReliSock *sock = (ReliSock *) arg;
    int stat;
    
    sock->encode();
    
    //send size of data to send
    stat = sock->put( size );
    
    
    //if successful, send the data
    if ( stat ) {
        // don't call code_bytes() on a zero-length buffer
        if ( size != 0 && !(stat = sock->code_bytes( buf, ((int) size )) ) ) {
            dprintf( D_ALWAYS, "failure sending data (%lu bytes) over sock\n",(unsigned long)size);
        }
    }
    else {
        dprintf( D_ALWAYS, "failure sending size (%lu) over sock\n", (unsigned long)size );
    }
    
    sock->end_of_message();
    
    //ensure data send was successful
    if ( stat == FALSE) {
        dprintf( D_ALWAYS, "relisock_gsi_put (write to socket) failure\n" );
        relisock_gsi_put_last_size = 0;
        return -1;
    }
    relisock_gsi_put_last_size = size;
    return 0;
}


int Sock::special_connect(char const *host,int /*port*/,bool nonblocking)
{
	if( !host || *host != '<' ) {
		return CEDAR_ENOCCB;
	}

	Sinful sinful(host);
	if( !sinful.valid() ) {
		return CEDAR_ENOCCB;
	}

	char const *shared_port_id = sinful.getSharedPortID();
	if( shared_port_id ) {
			// If the port of the SharedPortServer is 0, that means
			// we do not know the address of the SharedPortServer.
			// This happens, for example when Create_Process passes
			// the parent's address to a child or the child's address
			// to the parent and the SharedPortServer does not exist yet.
			// So do a local connection bypassing SharedPortServer,
			// if we are on the same machine.

			// Another case where we want to bypass connecting to
			// SharedPortServer is if we are the shared port server,
			// because this causes us to hang.

			// We could additionally bypass using the shared port
			// server if we use the same shared port server as the
			// target daemon and we are on the same machine.  However,
			// this causes us to use an additional ephemeral port than
			// if we connect to the shared port, so in case that is
			// important, use the shared port server instead.

		bool no_shared_port_server =
			sinful.getPort() && strcmp(sinful.getPort(),"0")==0;

		bool same_host = false;
		// TODO: Picking IPv4 arbitrarily.
		//   We should do a better job of detecting whether sinful
		//   points to a local interface.
		std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();
		if( sinful.getHost() && strcmp(my_ip.c_str(),sinful.getHost())==0 ) {
			same_host = true;
		}

		bool i_am_shared_port_server = false;
		if( daemonCore ) {
			char const *daemon_addr = daemonCore->publicNetworkIpAddr();
			if( daemon_addr ) {
				Sinful my_sinful(daemon_addr);
				if( my_sinful.getHost() && sinful.getHost() &&
					strcmp(my_sinful.getHost(),sinful.getHost())==0 &&
					my_sinful.getPort() && sinful.getPort() &&
					strcmp(my_sinful.getPort(),sinful.getPort())==0 &&
					(!my_sinful.getSharedPortID() ||
					 strcmp(my_sinful.getSharedPortID(),shared_port_id)==0) )
				{
					i_am_shared_port_server = true;
					dprintf(D_FULLDEBUG,"Bypassing connection to shared port server %s, because that is me.\n",daemon_addr);
				}
			}
		}
		if( (no_shared_port_server && same_host) || i_am_shared_port_server ) {

			if( no_shared_port_server && same_host ) {
				dprintf(D_FULLDEBUG,"Bypassing connection to shared port server, because its address is not yet established; passing socket directly to %s.\n",host);
			}

			// do_shared_port_local_connect() calls connect_socketpair(), which
			// normally uses loopback addresses.  However, the loopback address
			// may not be in the ALLOW list.  Instead, we need to use the
			// address we would use to contact the shared port daemon.
			const char * sharedPortIP = sinful.getHost();
			// Presently, for either same_host or i_am_shared_port_server to
			// be true, this must be as well.
			ASSERT( sharedPortIP );

			return do_shared_port_local_connect( shared_port_id, nonblocking, sharedPortIP );
		}
	}

		// Set shared port id even if it is null so we clear whatever may
		// already be there.  If it is not null, then this information
		// is saved here and used later after we have connected.
	setTargetSharedPortID( shared_port_id );

	char const *ccb_contact = sinful.getCCBContact();
	if( !ccb_contact || !*ccb_contact ) {
		return CEDAR_ENOCCB;
	}

	return do_reverse_connect(ccb_contact,nonblocking);
}

int
SafeSock::do_reverse_connect(char const *,bool)
{
	dprintf(D_ALWAYS,
			"CCBClient: WARNING: UDP not supported by CCB."
			"  Will therefore try to send packet directly to %s.\n",
			peer_description());

	return CEDAR_ENOCCB;
}

int
ReliSock::do_reverse_connect(char const *ccb_contact,bool nonblocking)
{
	ASSERT( !m_ccb_client.get() ); // only one reverse connect at a time!

	//
	// Since we can't change the CCB server without also changing the CCB
	// client (that is, without breaking backwards compatibility), we have
	// to determine if the server sent us ... a string we can't use.  Joy.
	//

	m_ccb_client =
		new CCBClient( ccb_contact, (ReliSock *)this );

	if( !m_ccb_client->ReverseConnect(NULL,nonblocking) ) {
		dprintf(D_ALWAYS,"Failed to reverse connect to %s via CCB.\n",
				peer_description());
		return 0;
	}
	if( nonblocking ) {
		return CEDAR_EWOULDBLOCK;
	}

	m_ccb_client = NULL; // in blocking case, we are done with ccb client
	return 1;
}

int
SafeSock::do_shared_port_local_connect( char const *, bool, char const * )
{
	dprintf(D_ALWAYS,
			"SharedPortClient: WARNING: UDP not supported."
			"  Failing to connect to %s.\n",
			peer_description());

	return 0;
}

int
ReliSock::do_shared_port_local_connect( char const *shared_port_id, bool nonblocking, char const *sharedPortIP )
{
		// Without going through SharedPortServer, we want to connect
		// to a daemon that is local to this machine and which is set up
		// to use the local SharedPortServer.  We do this by creating
		// a connected socket pair and then passing one of those sockets
		// to the target daemon over its named socket (or whatever mechanism
		// this OS supports).

	SharedPortClient shared_port_client;
	ReliSock sock_to_pass;
	std::string orig_connect_addr = get_connect_addr() ? get_connect_addr() : "";
	if( !connect_socketpair(sock_to_pass, sharedPortIP) ) {
		dprintf(D_ALWAYS,
				"Failed to connect to loopback socket, so failing to connect via local shared port access to %s.\n",
				peer_description());
		return 0;
	}

#if defined(DARWIN)
	//
	// See GT#7866.  Summary: removing the blocking acknowledgement of the
	// socket hand-off (#7502), if the master sleeps for 100ms instead of
	// re-entering the event loop (check-in [60471]), then -- only on
	// MacOS X and only for the shared port daemon -- the socket on which
	// the childalive message was sent will arrive at the master with no
	// data to read.  If other daemons are forced to use this function,
	// and attempt to contact the master while it's sleeping, they also fail.
	//
	// We have not been able to further analyze this problem, but dup()ing
	// the (newly-created) socket here works around the problem.
	//
	static int liveness_hack = -1;
	if( liveness_hack != -1 ) { ::close(liveness_hack); }
	liveness_hack = dup( sock_to_pass.get_file_desc() );
#endif

		// restore the original connect address, which got overwritten
		// in connect_socketpair()
	set_connect_addr(orig_connect_addr.c_str());

	char const *request_by = "";
	// A nonblocking call here causes a segfault, so don't do that.
	if( !shared_port_client.PassSocket(&sock_to_pass,shared_port_id,request_by, false) ) {
		return 0;
	}

	if( nonblocking ) {
			// We must pretend that we are not yet connected so that callers
			// who want a non-blocking connect will get the expected behavior
			// from Register_Socket() (register for write rather than read).
		_state = sock_connect_pending;
		return CEDAR_EWOULDBLOCK;
	}

	enter_connected_state();
	return 1;
}

void
ReliSock::cancel_reverse_connect() {
	ASSERT( m_ccb_client.get() );
	m_ccb_client->CancelReverseConnect();
}

bool
ReliSock::sendTargetSharedPortID()
{
	char const *shared_port_id = getTargetSharedPortID();
	if( !shared_port_id ) {
		return true;
	}
	SharedPortClient shared_port;
	return shared_port.sendSharedPortID(shared_port_id,this);
}

char const *
Sock::get_sinful_public() const
{
		// In case TCP_FORWARDING_HOST changes, do not cache it.
	std::string tcp_forwarding_host;
	param(tcp_forwarding_host,"TCP_FORWARDING_HOST");
	if (!tcp_forwarding_host.empty()) {
		condor_sockaddr addr;
		
		if (!addr.from_ip_string(tcp_forwarding_host)) {
			std::vector<condor_sockaddr> addrs = resolve_hostname(tcp_forwarding_host);
			if (addrs.empty()) {
				dprintf(D_ALWAYS,
					"failed to resolve address of TCP_FORWARDING_HOST=%s\n",
					tcp_forwarding_host.c_str());
				return NULL;
			}
			addr = addrs.front();
		}
		addr.set_port(get_port());
		_sinful_public_buf = addr.to_sinful().c_str();

		std::string alias;
		if( param(alias,"HOST_ALIAS") ) {
			Sinful s(_sinful_public_buf.c_str());
			s.setAlias(alias.c_str());
			_sinful_public_buf = s.getSinful();
		}

		return _sinful_public_buf.c_str();
	}

	return get_sinful();
}
