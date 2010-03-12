#ifndef DCLOUDGAHP_COMMANDS_H
#define DCLOUDGAHP_COMMANDS_H

typedef bool (*workerfn)(int argc, char **argv, MyString &output_string);

class DcloudGahpCommand
{
public:
    DcloudGahpCommand(const char *name, workerfn);
    MyString command;
    workerfn workerfunction;
};

extern FILE *logfp;

bool dcloud_start_worker(int argc, char **argv, MyString &output_string);
bool dcloud_action_worker(int argc, char **argv, MyString &output_string);
bool dcloud_info_worker(int argc, char **argv, MyString &output_string);
bool dcloud_statusall_worker(int argc, char **argv, MyString &output_string);
bool dcloud_find_worker(int argc, char **argv, MyString &output_string);

#endif
