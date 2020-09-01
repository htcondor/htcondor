
#ifndef __HISTORY_ITERATOR_H_
#define __HISTORY_ITERATOR_H_

bool putClassAdAndEOM(Sock & sock, classad::ClassAd &ad);
bool getClassAdWithoutGIL(Sock &sock, classad::ClassAd &ad);

struct HistoryIterator
{
	HistoryIterator(boost::shared_ptr<Sock> sock)
		: m_count(0), m_sock(sock)
	{}

	inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

	boost::shared_ptr<ClassAdWrapper> next()
	{
		if (m_count < 0) THROW_EX(StopIteration, "All ads processed");

		boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
		if (!getClassAdWithoutGIL(*m_sock.get(), *ad.get())) THROW_EX(HTCondorIOError, "Failed to receive remote ad.");
		long long intVal;
		if (ad->EvaluateAttrInt(ATTR_OWNER, intVal) && (intVal == 0))
		{ // Last ad.
			if (!m_sock->end_of_message()) THROW_EX(HTCondorIOError, "Unable to close remote socket");
			m_sock->close();
			std::string errorMsg;
			if (ad->EvaluateAttrInt(ATTR_ERROR_CODE, intVal) && intVal && ad->EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
			{
				THROW_EX(HTCondorIOError, errorMsg.c_str());
			}
			if (ad->EvaluateAttrInt("MalformedAds", intVal) && intVal) THROW_EX(HTCondorValueError, "Remote side had parse errors on history file")
				if (!ad->EvaluateAttrInt(ATTR_NUM_MATCHES, intVal) || (intVal != m_count)) THROW_EX(HTCondorValueError, "Incorrect number of ads returned");

			// Everything checks out!
			m_count = -1;
			THROW_EX(StopIteration, "All ads processed");
		}
		m_count++;
		return ad;
	}

private:
	int m_count;
	boost::shared_ptr<Sock> m_sock;
};

boost::shared_ptr<HistoryIterator>
history_query(boost::python::object requirement, boost::python::list projection, int match, boost::python::object since, int cmd, const std::string & addr);

#endif
