install ( FILES condor_credmon_oauth condor_credmon_vault DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
install ( FILES scitokens_credential_producer DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
install ( FILES condor_credential_storer DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
# condor_vault_storer was renamed to the mode-neutral condor_credential_storer
# (it now serves Vault, Pelican, ... services).  Keep a symlink under the old
# name so existing SEC_CREDENTIAL_STORER = .../condor_vault_storer configs and
# any scripts calling it directly keep working.
install ( CODE "EXECUTE_PROCESS(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink condor_credential_storer \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${C_BIN}/condor_vault_storer)" )
install ( FILES condor_credmon_oauth.wsgi DESTINATION ${C_LIBEXEC} )
install ( DIRECTORY credmon DESTINATION ${C_LIBEXEC} )
install ( DIRECTORY examples/ DESTINATION ${C_ETC_EXAMPLES}/condor_credmon_oauth )
install ( FILES README.credentials DESTINATION ${C_ETC_EXAMPLES}/condor_credmon_oauth )
