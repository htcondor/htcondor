dnl create symbolic links to lcmaps_gridlist.[ch] or lcas_gridlist.[ch] in various directories
dnl Usage:
dnl AC_GRIDLIST(PLUGIN-TYPE (LCAS OR LCMAPS), [DESTINATION DIRECTORY 1 [, DESTINATION DIRECTORY 2]]])
AC_DEFUN([AC_GRIDLIST],
[
    AC_ARG_WITH(module_location,
        [  --with-module-location=PFX     location of the current module. (current directory)],
        [],
        with_module_location=${PWD:-./})
        the_top_dir=${with_module_location}

        AC_MSG_RESULT(["Distributing gridlist files ..."])
        if test $# -ne 2 ; then \
            AC_MSG_RESULT(["Need these two parameters: lcas/lcmaps and directory name"]); \
        fi
        plugintype=$1
        dir="${the_top_dir}/src/$2"

        gridlistdir="${the_top_dir}/src/gridlist"
        for fname in "${plugintype}_gridlist.c" "${plugintype}_gridlist.h"
        do
            if test -d "${dir}" ; then
                ( \
                cd ${dir}; \
                if test -f ${gridlistdir}/${fname} ; then \
                    if test ! -h ${fname} -a ! -f ${fname} ; then \
                        AC_MSG_RESULT(["creating link to ${gridlistdir}/${fname} in ${dir}"]); \
                        ln -s ${gridlistdir}/${fname} ${fname}; \
                        AC_MSG_RESULT(["... success"]) ; \
                    else \
                        AC_MSG_RESULT(["... failed because link or regular file ${fname} already exists in ${dir}"]); \
                    fi \
                else \
                    AC_MSG_RESULT(["... failed because srcfile ${gridlistdir}/${fname} cannot be found to create symbolic link to in ${dir}"]); \
                fi \
                )
            else
                AC_MSG_RESULT(["... failed for ${fname} to ${dir}"])
            fi
        done
])
