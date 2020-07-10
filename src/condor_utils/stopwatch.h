#ifndef __STOPWATCH_H_
#define __STOPWATCH_H_

#include "config.h"

extern double _condor_debug_get_time_double();

class Stopwatch {
public:

    Stopwatch() : m_accum_ms(0.0), m_running(false)
    {
        reset();
    }

    ~Stopwatch() {}

    void start()
    {
        m_start_s = _condor_debug_get_time_double();
        m_running = true;
    }

    double stop()
    {
        m_accum_ms += (_condor_debug_get_time_double() - m_start_s)*1000;
        m_running = false;
        return m_accum_ms;
    }

    double get_ms() const
    {
        double change_ms = 0;
        if (m_running)
        {
            change_ms = (_condor_debug_get_time_double() - m_start_s)*1000;
        }
        return m_accum_ms + change_ms;
    }


    void reset()
    {
        m_start_s = 0;
        m_accum_ms = 0;
    }


    bool is_running() const {return m_running;}

private:

    double m_start_s;
    double m_accum_ms;
    bool m_running;

};

class StopwatchSentry
{
public:
	StopwatchSentry(Stopwatch &watch)
		: m_started(!watch.is_running()),
		  m_watch(watch)
	{
		if (m_started) {watch.start();}
	}

	~StopwatchSentry()
	{
		if (m_started) {m_watch.stop();}
	}

private:
	bool m_started;
	Stopwatch &m_watch;
};

#endif
