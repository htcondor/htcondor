AC_DEFUN([AC_GLITE_DOCBOOK],
[

    AC_ARG_WITH(glite_docbook_prefix,
                [--with-glite-docbook-prefix=PFX    prefix for the style sheet root directory],
                [],
                with_glite_prefix=/usr/share/sgml/docbook/xsl-stylesheets)

    AC_MSG_CHECKING([for man pages stylesheet])
    
    dnl test for sl*
    if test -e ${with_glite_prefix}/manpages/docbook.xsl; then
        GLITE_DB_MANPAGES_STYLESHEET=${with_glite_prefix}/manpages/docbook.xsl
        AC_MSG_RESULT([yes])
    else
        dnl test for deb6
        with_glite_prefix=/usr/share/xml/docbook/stylesheet/docbook-xsl
        if test -e ${with_glite_prefix}/manpages/docbook.xsl; then
            GLITE_DB_MANPAGES_STYLESHEET=${with_glite_prefix}/manpages/docbook.xsl
            AC_MSG_RESULT([yes])
        else
            GLITE_DB_MANPAGES_STYLESHEET=""
            AC_MSG_ERROR([no])
        fi
    fi

    AC_SUBST(GLITE_DB_MANPAGES_STYLESHEET)

])
