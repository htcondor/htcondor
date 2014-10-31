#ifndef __STOPWATCH_H_
#define __STOPWATCH_H_

#include "config.h"

// Yes - clock_gettime is significantly faster than gettimeofday.
// The overhead of clock_gettime is around 30ns or less; still would
// recommend to using this sparingly!
#ifdef HAVE_CLOCK_MONOTONIC
#define STOPWATCH_CLOCK_MONOTONIC
#endif

class Stopwatch {
public:

    Stopwatch() : m_accum_ms(0.0), m_running(false)
    {
        reset();
    }

    ~Stopwatch() {}

    void start()
    {
#ifdef STOPWATCH_CLOCK_MONOTONIC
        clock_gettime(CLOCK_MONOTONIC_RAW, &m_start);
#else
        gettimeofday(&m_start, NULL);
#endif
        m_running = true;
    }

    double stop()
    {
#ifdef STOPWATCH_CLOCK_MONOTONIC
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
        m_accum_ms += static_cast<double>(tp.tv_sec - m_start.tv_sec)*1000. + static_cast<double>(tp.tv_nsec - m_start.tv_nsec)/1000000.;
#else
        struct timeval tp;
        gettimeofday(&tp, 0);
        m_accum_ms += static_cast<double>(tp.tv_sec + m_start.tv_sec)*1000. + static_cast<double>(tp.tv_usec - m_start.tv_usec)/1000.;
#endif
        m_running = false;
        return m_accum_ms;
    }

    double get_ms()
    {
        double change = 0;
        if (m_running)
        {
#ifdef STOPWATCH_CLOCK_MONOTONIC
            struct timespec tp;
            clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
            change = static_cast<double>(tp.tv_sec - m_start.tv_sec)*1000. + static_cast<double>(tp.tv_nsec - m_start.tv_nsec)/1000000.;
#else
            struct timeval tp;
            gettimeofday(&tp, 0);
            change = static_cast<double>(tp.tv_sec + m_start.tv_sec)*1000. + static_cast<double>(tp.tv_usec - m_start.tv_usec)/1000.;
#endif
        }
        return m_accum_ms + change;
    }


    void reset()
    {
#ifdef STOPWATCH_CLOCK_MONOTONIC
        m_start.tv_sec = 0;
        m_start.tv_nsec = 0;
#else
        m_start.tv_sec = 0;
        m_start.tv_usec = 0;
#endif
    }

private:

#ifdef STOPWATCH_CLOCK_MONOTONIC
    struct timespec m_start;
#else
    struct timeval m_start;
#endif
    double m_accum_ms;
    bool m_running;

};

#endif
