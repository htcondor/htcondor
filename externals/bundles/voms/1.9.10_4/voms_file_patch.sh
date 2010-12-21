#!/bin/sh

# Voms includes a modified version of the GSI oldgaa library, using
# the same function names as the original. We need to statically link
# against both, which produces sybmol conflicts. So we rename all of
# the symbols in the Voms version, which aren't exposed in the Voms
# API.
files="../sslutils.c globus_oldgaa.h globus_oldgaa_utils.c \
    globus_oldgaa_utils.h oldgaa_alloc.c oldgaa_api.c \
    oldgaa_gl_internal_err.c oldgaa_gl_internal_err.h \
    oldgaa_policy_evaluator.c oldgaa_policy_evaluator.h 
    oldgaa_release.c oldgaa_utils.c oldgaa_utils.h rfc1779.c"
funcs="oldgaa_globus_allocate_rights \
    oldgaa_globus_allocate_sec_context \
    oldgaa_globus_cleanup \
    oldgaa_globus_get_trusted_ca_list \
    oldgaa_globus_initialize \
    oldgaa_globus_parse_conditions \
    oldgaa_globus_parse_policy \
    oldgaa_globus_parse_principals \
    oldgaa_globus_parse_rights \
    oldgaa_globus_policy_file_close \
    oldgaa_globus_policy_file_open \
    oldgaa_globus_policy_retrieve \
    oldgaa_allocate_answer \
    oldgaa_allocate_buffer \
    oldgaa_allocate_cond_bindings \
    oldgaa_allocate_conditions \
    oldgaa_allocate_data \
    oldgaa_allocate_identity_cred \
    oldgaa_allocate_options \
    oldgaa_allocate_principals \
    oldgaa_allocate_rights \
    oldgaa_allocate_sec_attrb \
    oldgaa_allocate_sec_context \
    oldgaa_check_authorization \
    oldgaa_get_object_policy_info \
    oldgaa_inquire_policy_info \
    oldgaa_check_access_rights \
    oldgaa_evaluate_conditions \
    oldgaa_evaluate_day_cond \
    oldgaa_evaluate_regex_cond \
    oldgaa_evaluate_sech_mech_cond \
    oldgaa_evaluate_time_cond \
    oldgaa_find_matching_entry \
    oldgaa_get_authorized_principals \
    oldgaa_release_answer \
    oldgaa_release_attributes \
    oldgaa_release_authr_cred \
    oldgaa_release_buffer \
    oldgaa_release_buffer_contents \
    oldgaa_release_cond_bindings \
    oldgaa_release_conditions \
    oldgaa_release_data \
    oldgaa_release_identity_cred \
    oldgaa_release_options \
    oldgaa_release_principals \
    oldgaa_release_rights \
    oldgaa_release_sec_attrb \
    oldgaa_release_sec_context \
    oldgaa_release_uneval_cred \
    oldgaa_add_attribute \
    oldgaa_add_cond_binding \
    oldgaa_add_condition \
    oldgaa_add_principal \
    oldgaa_add_rights \
    oldgaa_bind_rights_to_conditions \
    oldgaa_bind_rights_to_principals \
    oldgaa_check_reg_expr \
    oldgaa_collapse_policy \
    oldgaa_compare_conditions \
    oldgaa_compare_principals \
    oldgaa_compare_rights \
    oldgaa_compare_sec_attrbs \
    oldgaa_handle_error \
    oldgaa_parse_regex \
    oldgaa_regex_matches_string \
    oldgaa_strcopy \
    oldgaa_strings_match \
    string_count \
    string_max \
    oldgaa_rfc1779_name_parse \
    internal_error_handler \
    oldgaa_gl__fout_of_memory \
    oldgaa_gl__function_internal_error_helper \
    oldgaa_gl__is_out_of_memory \
    oldgaa_gl_out_of_memory_handler"

cd org.glite.security.voms/src/sslutils/oldgaa
for func in $funcs ; do
  for file in $files ; do
    echo "altering $file -> voms_$func \n"
    sed -i -e "s/$func/voms_$func/" $file 
  done
done
