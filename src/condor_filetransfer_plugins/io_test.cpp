#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "get_daemon_name.h"
#include "daemon.h"
#include "subsystem_info.h"
#include "CondorError.h"
#include "string_list.h"

#include "condor_hdfs_io.h"

DECL_SUBSYSTEM( "TOOL", SUBSYSTEM_TYPE_TOOL );

#define TMP_DIR "/scratch"
#define DEFAULT_FILE_SIZE_MB 100
#define FILE_SIZE_SKEW_MB   10

static StringList fileList;

void test_get_file() {
    HdfsIO hdfsIO;

    CondorError error;
    const char * fileName = fileList.next();
    assert (fileName != NULL);
    printf("\n\tCopying... : %s\n", fileName);
    int ret = hdfsIO.get_file(fileName, fileName, error);

    do {
        printf ("\t\tret =  %d, msg = %s\n", ret, error.message());
    } while (error.pop());

    unlink(fileName);
}

void test_put_file() {
    HdfsIO hdfsIO;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    MyString fileName;
    double usecs = (double) ((tv.tv_sec * 1e6) + tv.tv_usec);
    fileName.sprintf("%s/hdfs_test_%ld", TMP_DIR, usecs);
    fileList.insert(fileName.GetCStr());

    long filesize = DEFAULT_FILE_SIZE_MB * 1024 * 1024;
    filesize = filesize + (rand() % (FILE_SIZE_SKEW_MB * 1024 * 1024));

    //let's generate a temperory file
    int srcfd = safe_open_wrapper("/dev/zero", O_RDONLY);
    assert(srcfd != -1);
    int destfd = safe_open_wrapper(fileName.GetCStr(), O_WRONLY|O_CREAT);
    assert(destfd != -1);

    long nwrote = 0;
    char buffer[4096];
    while (nwrote < filesize) {
        long nread = read(srcfd, buffer, 4096);
        assert (nread > 0);
        nwrote += write(destfd, buffer, nread);
    }

    close(srcfd);
    close(destfd);
   
    CondorError error;
    printf("\n\tTransfering: %s (%f MB)\n", fileName.GetCStr(), (double)(filesize/(1024 * 1024)));
    int ret = hdfsIO.put_file(fileName.GetCStr(), fileName.GetCStr(), error);
    do {
        printf ("\t\tret =  %d, msg = %s\n\n", ret, error.message());
    } while (error.pop());


    unlink(fileName.GetCStr());
}


int main(int argc, char *argv[]) {
    int num_tests = 5;

    config();

    int i;
    printf("\n\n put_test \n\n");
    for (i=0; i<num_tests; i++) {
        test_put_file();
    }

    /*fileList.rewind();
    printf("\n\n get_test \n\n");
    for (i=0; i<num_tests; i++) {
        test_get_file();
    } */
}
