#ifndef DCLOUDGAHP_COMMANDS_H
#define DCLOUDGAHP_COMMANDS_H

typedef bool (*workerfn)(int argc, char **argv, std::string &output_string);

class DcloudGahpCommand
{
public:
    DcloudGahpCommand(const char *name, workerfn);
    std::string command;
    workerfn workerfunction;
};

extern FILE *logfp;

bool dcloud_start_worker(int argc, char **argv, std::string &output_string);
bool dcloud_action_worker(int argc, char **argv, std::string &output_string);
bool dcloud_info_worker(int argc, char **argv, std::string &output_string);
bool dcloud_statusall_worker(int argc, char **argv, std::string &output_string);
bool dcloud_find_worker(int argc, char **argv, std::string &output_string);
bool dcloud_max_name_length_worker(int argc, char **argv,
                                   std::string &output_string);
bool dcloud_start_auto_worker(int argc, char **argv,
                              std::string &output_string);

#endif
