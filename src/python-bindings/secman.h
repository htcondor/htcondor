/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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

#ifndef __SECMAN_H_
#define __SECMAN_H_

class ConfigOverrides;
class Token;

class CapabilitySet;

class CapabilityToken {
public:
    CapabilityToken(int command, const std::string &server,
         const std::string &value, const std::string &info)
       : m_command(command),
         m_server(server),
         m_value(value),
         m_info(info)
    {}

    CapabilityToken(boost::python::object command, const std::string &server,
    const std::string &value, const std::string &info);

    int command() const {return m_command;}
    std::string commandString() const;
    std::string server() const {return m_server;}
    std::string get() const {return m_value;}
    std::string getPython() const {return pythonFormat(m_value);}
    std::string info() const {return m_info;}

    static std::string pythonFormat(const std::string &);
    static std::string condorFormat(const std::string &);
private:
    const int m_command; // Command integer for this capability
    const std::string m_server; // The sinful string for the remote host.
    const std::string m_value;  // The actual capability contents
    const std::string m_info;   // The session info we want for this token.
};

struct SecManWrapper
{
public:
    SecManWrapper();

    void invalidateAllCache();
    boost::shared_ptr<ClassAdWrapper> ping(boost::python::object locate_obj, boost::python::object command_obj=boost::python::object("DC_NOP"));

    std::string getCommandString(int cmd) {return getCommandStringStatic(cmd);}
    static std::string getCommandStringStatic(int cmd);
    int getCommandNumber(const std::string &cmd);

    boost::python::object getCapabilityTokens(boost::python::object locate_obj,
        boost::python::object req_list, int duration);

    static boost::shared_ptr<SecManWrapper> enter(boost::shared_ptr<SecManWrapper> obj);
    bool exit(boost::python::object, boost::python::object, boost::python::object);

    static const char * getThreadLocalTag();
    static const char * getThreadLocalPoolPassword();
    static const char * getThreadLocalGSICred();
    static const char * getThreadLocalToken();
    static CapabilitySet *getThreadLocalCapability();
    static bool applyThreadLocalConfigOverrides(ConfigOverrides & old);

    void setToken(const Token &);
    void setTag(const std::string &tag);
    void setPoolPassword(const std::string &pool_pass);
    void setCapability(const CapabilityToken &capability);
    void setGSICredential(const std::string &cred);
    void setConfig(const std::string &key, const std::string &val);

private:
    SecMan m_secman;
    static MODULE_LOCK_TLS_KEY m_key;
    static bool m_key_allocated;
    std::string m_tag;
    std::string m_pool_pass;
    std::string m_token;
    std::shared_ptr<CapabilitySet> m_capability;
    std::string m_cred;
    ConfigOverrides m_config_overrides;
    bool m_tag_set;
    bool m_pool_pass_set;
    bool m_cred_set;
    bool m_token_set{false};
    bool m_capability_set{false};
};

#endif  // __SECMAN_H_
