//#include "util_lib_proto.h"
#include "sandbox_manager.h"

int main(void) {
    const char dir[] = "/tmp";

    printf ("=======================================================\n");
    printf ("                       TESTING\n");
    printf ("=======================================================\n");
    printf ("Creating new sandbox manager...\n");
    CSandboxManager *sm = new CSandboxManager();
    printf ("Creating new sandbox manager... DONE\n");

    printf ("Registering new sandbox %s ...\n", dir);
    printf ("Sandbox Id: %s\n", sm->registerSandbox(dir));
    printf ("Registering new sandbox ... DONE\n");

    printf ("Registering new sandbox %s ...\n", dir);
    printf ("Sandbox Id: %s\n", sm->registerSandbox(dir));
    printf ("Registering new sandbox ... DONE\n");

    printf ("Registering new sandbox %s ...\n", dir);
    printf ("Sandbox Id: %s\n", sm->registerSandbox(dir));
    printf ("Registering new sandbox ... DONE\n");

    printf ("Listing registered sandboxes ...\n");
    sm->listRegisteredSandboxes();
    printf ("Listing registered sandboxes ... DONE\n");

}
