
#include "classad_wrapper.h"

enum ParserType {
  CLASSAD_AUTO,
  CLASSAD_OLD,
  CLASSAD_NEW,
  CLASSAD_JSON
};

ClassAdWrapper *parseString(const std::string &str);
ClassAdWrapper *parseFile(FILE *stream);
ClassAdWrapper *parseOld(boost::python::object input);
boost::shared_ptr<ClassAdWrapper> parseOne(boost::python::object input, ParserType);
boost::python::object parseAds(boost::python::object input, ParserType type);
boost::python::object parseNext(boost::python::object input, ParserType type);

class OldClassAdIterator
{
public:

    OldClassAdIterator(boost::python::object source);

    static boost::python::object pass_through(boost::python::object const& o);

    boost::shared_ptr<ClassAdWrapper> next();

private:
    bool m_done;
    bool m_source_has_next;
    boost::shared_ptr<ClassAdWrapper> m_ad;
    boost::python::object m_source;
};

OldClassAdIterator parseOldAds(boost::python::object input);

class ClassAdFileIterator
{
public:
    ClassAdFileIterator() : m_done(true), m_source(NULL) {}
    ClassAdFileIterator(FILE *source);

    boost::shared_ptr<ClassAdWrapper> next();

private:
    bool m_done;
    FILE * m_source;
    boost::shared_ptr<classad::ClassAdParser> m_parser;
};

class ClassAdStringIterator
{
public:

    ClassAdStringIterator(const std::string & source);

    boost::shared_ptr<ClassAdWrapper> next();

private:
    int m_off;
    std::string m_source;
    boost::shared_ptr<classad::ClassAdParser> m_parser;
};

ClassAdFileIterator parseAdsFile(FILE *);

ClassAdStringIterator parseAdsString(const std::string &);

