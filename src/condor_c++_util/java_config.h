#ifndef JAVA_CONFIG_H
#define JAVA_CONFIG_H

/*
Extract the java configuration from the local config files.
The name of the java executable gets put in 'cmd', and the necessary
arguments get put in 'args'.  If you have other dirs or jarfiles
that should be placed in the classpath, provide them in 'extra_classpath'.
*/

int java_config( char *cmd, char *args, StringList *extra_classpath );

#endif
