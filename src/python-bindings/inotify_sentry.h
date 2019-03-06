
#ifndef _INOTIFY_SENTRY_H
#define _INOTIFY_SENTRY_H

#include "condor_common.h"

#include <string>
#include <errno.h>

#ifdef HAVE_INOTIFY
#define LOG_READER_USE_INOTIFY
#endif

#ifdef LOG_READER_USE_INOTIFY
#include <sys/inotify.h>
#define INOTIFY_BUFSIZE sizeof(struct inotify_event)
#else
#define INOTIFY_BUFSIZE 20
#endif

class InotifySentry {

public:
    InotifySentry(const std::string &fname) : m_fd(-1)
    {
#ifdef LOG_READER_USE_INOTIFY
        if ((m_fd = inotify_init()) == -1)
        {
            THROW_EX(HTCondorIOError, "Failed to create inotify instance.");
        }
        fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        fcntl(m_fd, F_SETFL, O_NONBLOCK);

        if (inotify_add_watch(m_fd, fname.c_str(), IN_MODIFY | IN_ATTRIB | IN_DELETE_SELF) == -1)
        {
            THROW_EX(HTCondorIOError, "Failed to add inotify watch.");
        }
#else
        if (fname.c_str()) {}
#endif
    }

    ~InotifySentry() {if (m_fd >= 0) {close(m_fd);}}
    int watch() const {return m_fd;}

    int clear()
    {
        if (m_fd == -1) {return -1;}
        int events = 0;
#ifdef LOG_READER_USE_INOTIFY
        struct inotify_event event;
        int size, count = 0;
        errno = 0;
        do
        {
            if (errno)
            {
                THROW_EX(HTCondorIOError, "Failure when reading the inotify event buffer.");
            }
            do
            {
                size = read(m_fd, reinterpret_cast<char *>(&event)+count, INOTIFY_BUFSIZE-count);
                count += size;
            }
            while ((count != INOTIFY_BUFSIZE) && (size != -1 || errno == EINTR));
            count = 0;
            events++;
            assert(event.len == 0);
        }
        while (errno != EAGAIN && errno != EWOULDBLOCK);
#endif
        return --events;
    }

private:
    int m_fd;
};

#endif

