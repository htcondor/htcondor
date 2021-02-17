/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

/*
	This code tests the old classads implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"

#ifdef WIN32
	#define strcasecmp _stricmp
#endif

static bool test_copy_constructor_actuals(void);
static bool test_copy_constructor_pointer(void);
static bool test_assignment_actuals(void);
static bool test_assignment_actuals_before(void);
static bool test_assignment_pointer(void);
static bool test_xml(void);
static bool test_lookup_integer_first(void);
static bool test_lookup_integer_middle(void);
static bool test_lookup_integer_last(void);
static bool test_lookup_bool_true(void);
static bool test_lookup_bool_false(void);
static bool test_lookup_expr_error_or_false(void);
static bool test_lookup_expr_error_and(void);
static bool test_lookup_expr_error_and_true(void);
static bool test_lookup_string_normal(void);
static bool test_lookup_string_long(void);
static bool test_lookup_string_file(void);
static bool test_get_my_type_name_no(void);
static bool test_get_my_type_name_yes(void);
static bool test_get_target_type_name_no(void);
static bool test_get_target_type_name_yes(void);
static bool test_is_a_match_true(void);
static bool test_is_a_match_true_reverse(void);
static bool test_is_a_match_false_memory(void);
static bool test_is_a_match_false_memory_reverse(void);
static bool test_is_a_match_false_owner(void);
static bool test_is_a_match_false_owner_reverse(void);
static bool test_expr_tree_to_string_short(void);
static bool test_expr_tree_to_string_long(void);
static bool test_expr_tree_to_string_long2(void);
static bool test_expr_tree_to_string_big(void);
static bool test_get_references_simple_true_internal(void);
static bool test_get_references_simple_true_external(void);
static bool test_get_references_simple_false_internal(void);
static bool test_get_references_simple_false_external(void);
static bool test_get_references_complex_true_internal(void);
static bool test_get_references_complex_true_external(void);
static bool test_get_references_complex_false_internal(void);
static bool test_get_references_complex_false_external(void);
static bool test_reference_name_trimming_internal(void);
static bool test_reference_name_trimming_external(void);
static bool test_next_dirty_expr_clear(void);
static bool test_next_dirty_expr_insert(void);
static bool test_next_dirty_expr_insert_two_calls(void);
static bool test_next_dirty_expr_two_inserts_first(void);
static bool test_next_dirty_expr_two_inserts_second(void);
static bool test_next_dirty_expr_two_inserts_third(void);
static bool test_next_dirty_expr_two_inserts_clear(void);
static bool test_next_dirty_expr_set_first(void);
static bool test_next_dirty_expr_set_second(void);
static bool test_get_dirty_flag_exists_dirty(void);
static bool test_get_dirty_flag_exists_not_dirty(void);
static bool test_get_dirty_flag_not_exist(void);
static bool test_from_file(void);
static bool test_init_from_string_ints(void);
static bool test_init_from_string_reals(void);
static bool test_init_from_string_if_then_else(void);
static bool test_init_from_string_string_list(void);
static bool test_init_from_string_string(void);
static bool test_init_from_string_strcat(void);
static bool test_init_from_string_floor(void);
static bool test_init_from_string_ceiling(void);
static bool test_init_from_string_round(void);
static bool test_init_from_string_random(void);
static bool test_init_from_string_is_string(void);
static bool test_init_from_string_is_undefined(void);
static bool test_init_from_string_is_error(void);
static bool test_init_from_string_is_integer(void);
static bool test_init_from_string_is_real(void);
static bool test_init_from_string_is_boolean(void);
static bool test_init_from_string_substr(void);
static bool test_init_from_string_formattime(void);
static bool test_init_from_string_strcmp(void);
static bool test_init_from_string_attrnm(void);
static bool test_init_from_string_regexp(void);
static bool test_init_from_string_stringlists_regexpmember(void);
static bool test_init_from_string_various(void);
static bool test_int_invalid(void);
static bool test_int_false(void);
static bool test_int_true(void);
static bool test_int_float_negative_quotes(void);
static bool test_int_float_negative(void);
static bool test_int_float_positive(void);
static bool test_int_int_positive(void);
static bool test_int_error(void);
static bool test_real_invalid(void);
static bool test_real_false(void);
static bool test_real_true(void);
static bool test_real_float_negative_quotes(void);
static bool test_real_float_negative(void);
static bool test_real_float_positive(void);
static bool test_real_int_positive(void);
static bool test_real_error(void);
static bool test_if_then_else_false(void);
static bool test_if_then_else_false_error(void);
static bool test_if_then_else_false_constant(void);
static bool test_if_then_else_true(void);
static bool test_if_then_else_true_error(void);
static bool test_if_then_else_true_constant1(void);
static bool test_if_then_else_true_constant2(void);
static bool test_if_then_else_invalid1(void);
static bool test_if_then_else_invalid2(void);
static bool test_if_then_else_too_few(void);
static bool test_if_then_else_undefined(void);
static bool test_string_list_size_3(void);
static bool test_string_list_size_0(void);
static bool test_string_list_size_5(void);
static bool test_string_list_size_default_delim(void);
static bool test_string_list_size_repeat_delim(void);
static bool test_string_list_size_non_default_delim(void);
static bool test_string_list_size_two_delim(void);
static bool test_string_list_size_three_delim(void);
static bool test_string_list_size_error_end(void);
static bool test_string_list_size_error_beginning(void);
static bool test_string_list_sum_default(void);
static bool test_string_list_sum_empty(void);
static bool test_string_list_sum_non_default(void);
static bool test_string_list_sum_both(void);
static bool test_string_list_sum_error_end(void);
static bool test_string_list_sum_error_beginning(void);
static bool test_string_list_sum_error_all(void);
static bool test_string_list_min_negative(void);
static bool test_string_list_min_positive(void);
static bool test_string_list_min_both(void);
static bool test_string_list_min_undefined(void);
static bool test_string_list_min_error_end(void);
static bool test_string_list_min_error_beginning(void);
static bool test_string_list_min_error_all(void);
static bool test_string_list_min_error_middle(void);
static bool test_string_list_max_negatve(void);
static bool test_string_list_max_positive(void);
static bool test_string_list_max_both(void);
static bool test_string_list_max_undefined(void);
static bool test_string_list_max_error_end(void);
static bool test_string_list_max_error_beginning(void);
static bool test_string_list_max_error_all(void);
static bool test_string_list_max_error_middle(void);
static bool test_string_list_avg_default(void);
static bool test_string_list_avg_empty(void);
static bool test_string_list_avg_non_default(void);
static bool test_string_list_avg_both(void);
static bool test_string_list_avg_error_end(void);
static bool test_string_list_avg_error_beginning(void);
static bool test_string_list_avg_error_all(void);
static bool test_string_list_avg_error_middle(void);
static bool test_string_list_member(void);
static bool test_string_list_member_case(void);
static bool test_string_negative_int(void);
static bool test_string_positive_int(void);
static bool test_strcat_short(void);
static bool test_strcat_long(void);
static bool test_floor_negative_int(void);
static bool test_floor_negative_float(void);
static bool test_floor_positive_integer(void);
static bool test_floor_positive_integer_wo(void);
static bool test_floor_positive_float_wo(void);
static bool test_ceiling_negative_int(void);
static bool test_ceiling_negative_float(void);
static bool test_ceiling_positive_integer(void);
static bool test_ceiling_positive_integer_wo(void);
static bool test_ceiling_positive_float_wo(void);
static bool test_round_negative_int(void);
static bool test_round_negative_float(void);
static bool test_round_positive_int(void);
static bool test_round_positive_float_wo_up(void);
static bool test_round_positive_float_wo_down(void);
static bool test_random_integer(void);
static bool test_random(void);
static bool test_random_float(void);
static bool test_is_string_simple(void);
static bool test_is_string_concat(void);
static bool test_is_undefined_true(void);
static bool test_is_undefined_false(void);
static bool test_is_error_random(void);
static bool test_is_error_int(void);
static bool test_is_error_real(void);
static bool test_is_error_floor(void);
static bool test_is_integer_false_negative(void);
static bool test_is_integer_false_positive(void);
static bool test_is_integer_false_quotes(void);
static bool test_is_integer_true_negative(void);
static bool test_is_integer_true_int(void);
static bool test_is_integer_true_int_quotes(void);
static bool test_is_integer_true_positive(void);
static bool test_is_real_false_negative_int(void);
static bool test_is_real_false_negative_int_quotes(void);
static bool test_is_real_false_positive_int(void);
static bool test_is_real_true_negative(void);
static bool test_is_real_true_positive(void);
static bool test_is_real_true_real(void);
static bool test_is_real_true_real_quotes(void);
static bool test_is_real_error(void);
static bool test_is_boolean(void);
static bool test_substr_end(void);
static bool test_substr_middle(void);
static bool test_substr_negative_index(void);
static bool test_substr_negative_length(void);
static bool test_substr_out_of_bounds(void);
static bool test_substr_error_index(void);
static bool test_substr_error_string(void);
static bool test_formattime_empty(void);
static bool test_formattime_current(void);
static bool test_formattime_current_options(void);
static bool test_formattime_int(void);
static bool test_formattime_error(void);
static bool test_strcmp_positive(void);
static bool test_strcmp_negative(void);
static bool test_strcmp_equal(void);
static bool test_strcmp_convert1(void);
static bool test_strcmp_convert2(void);
static bool test_stricmp_positive(void);
static bool test_stricmp_negative(void);
static bool test_stricmp_equal(void);
static bool test_stricmp_convert1(void);
static bool test_stricmp_convert2(void);
static bool test_regexp_match_wildcard(void);
static bool test_regexp_match_repeat(void);
static bool test_regexp_no_match(void);
static bool test_regexp_no_match_case(void);
static bool test_regexp_error_pattern(void);
static bool test_regexp_error_target(void);
static bool test_regexp_error_option(void);
static bool test_regexps_match(void);
static bool test_regexps_match_case(void);
static bool test_regexps_error_pattern(void);
static bool test_regexps_error_target(void);
static bool test_regexps_error_option(void);
static bool test_regexps_error_return(void);
static bool test_regexps_full(void);
static bool test_regexps_full_global(void);
static bool test_replace(void);
static bool test_replaceall(void);
static bool test_stringlist_regexp_member_match_default(void);
static bool test_stringlist_regexp_member_match_non_default(void);
static bool test_stringlist_regexp_member_match_case(void);
static bool test_stringlist_regexp_member_match_repeat(void);
static bool test_stringlist_regexp_member_no_match_multiple(void);
static bool test_stringlist_regexp_member_no_match(void);
static bool test_stringlist_regexp_member_no_match_case(void);
static bool test_stringlist_regexp_member_error_pattern(void);
static bool test_stringlist_regexp_member_error_target(void);
static bool test_stringlist_regexp_member_error_delim(void);
static bool test_stringlist_regexp_member_error_option(void);
static bool test_random_different(void);
static bool test_random_range(void);
static bool test_equality(void);
static bool test_inequality(void);
static bool test_operators_short_or(void);
static bool test_operators_no_short_or(void);
static bool test_operators_short_and(void);
static bool test_operators_short_and_error(void);
static bool test_operators_no_short_and(void);
static bool test_scoping_my(void);
static bool test_scoping_my_dup(void);
static bool test_scoping_target(void);
static bool test_scoping_target_dup(void);
static bool test_scoping_both(void);
static bool test_scoping_my_miss(void);
static bool test_scoping_target_miss(void);
static bool test_time(void);
static bool test_interval_minute(void);
static bool test_interval_hour(void);
static bool test_interval_day(void);
static bool test_to_upper(void);
static bool test_to_lower(void);
static bool test_size_positive(void);
static bool test_size_zero(void);
static bool test_size_undefined(void);
static bool test_nested_ads(void);


bool OTEST_Old_Classads(void) {
	emit_object("Old_Classads");
	
	FunctionDriver driver;
	driver.register_function( test_copy_constructor_actuals);
	driver.register_function( test_copy_constructor_pointer);
	driver.register_function(test_assignment_actuals);
	driver.register_function(test_assignment_actuals_before);
	driver.register_function(test_assignment_pointer);
	driver.register_function(test_xml);
	driver.register_function(test_lookup_integer_first);
	driver.register_function(test_lookup_integer_middle);
	driver.register_function(test_lookup_integer_last);
	driver.register_function(test_lookup_bool_true);
	driver.register_function(test_lookup_bool_false);
	driver.register_function(test_lookup_expr_error_or_false);
	driver.register_function(test_lookup_expr_error_and);
	driver.register_function(test_lookup_expr_error_and_true);
	driver.register_function(test_lookup_string_normal);
	driver.register_function(test_lookup_string_long);
	driver.register_function(test_lookup_string_file);
	driver.register_function(test_get_my_type_name_no);
	driver.register_function(test_get_my_type_name_yes);
	driver.register_function(test_get_target_type_name_no);
	driver.register_function(test_get_target_type_name_yes);
	driver.register_function(test_is_a_match_true);
	driver.register_function(test_is_a_match_true_reverse);
	driver.register_function(test_is_a_match_false_memory);
	driver.register_function(test_is_a_match_false_memory_reverse);
	driver.register_function(test_is_a_match_false_owner);
	driver.register_function(test_is_a_match_false_owner_reverse);
	driver.register_function(test_expr_tree_to_string_short);
	driver.register_function(test_expr_tree_to_string_long);
	driver.register_function(test_expr_tree_to_string_long2);
	driver.register_function(test_expr_tree_to_string_big);
	driver.register_function(test_get_references_simple_true_internal);
	driver.register_function(test_get_references_simple_true_external);
	driver.register_function(test_get_references_simple_false_internal);
	driver.register_function(test_get_references_simple_false_external);
	driver.register_function(test_get_references_complex_true_internal);
	driver.register_function(test_get_references_complex_true_external);
	driver.register_function(test_get_references_complex_false_internal);
	driver.register_function(test_get_references_complex_false_external);
	driver.register_function(test_reference_name_trimming_internal);
	driver.register_function(test_reference_name_trimming_external);
	driver.register_function(test_next_dirty_expr_clear);
	driver.register_function(test_next_dirty_expr_insert);
	driver.register_function(test_next_dirty_expr_insert_two_calls);
	driver.register_function(test_next_dirty_expr_two_inserts_first);
	driver.register_function(test_next_dirty_expr_two_inserts_second);
	driver.register_function(test_next_dirty_expr_two_inserts_third);
	driver.register_function(test_next_dirty_expr_two_inserts_clear);
	driver.register_function(test_next_dirty_expr_set_first);
	driver.register_function(test_next_dirty_expr_set_second);
	driver.register_function(test_get_dirty_flag_exists_dirty);
	driver.register_function(test_get_dirty_flag_exists_not_dirty);
	driver.register_function(test_get_dirty_flag_not_exist);
	driver.register_function(test_from_file);
	driver.register_function(test_init_from_string_ints);
	driver.register_function(test_init_from_string_reals);
	driver.register_function(test_init_from_string_if_then_else);
	driver.register_function(test_init_from_string_string_list);
	driver.register_function(test_init_from_string_string);
	driver.register_function(test_init_from_string_strcat);
	driver.register_function(test_init_from_string_floor);
	driver.register_function(test_init_from_string_ceiling);
	driver.register_function(test_init_from_string_round);
	driver.register_function(test_init_from_string_random);
	driver.register_function(test_init_from_string_is_string);
	driver.register_function(test_init_from_string_is_undefined);
	driver.register_function(test_init_from_string_is_error);
	driver.register_function(test_init_from_string_is_integer);
	driver.register_function(test_init_from_string_is_real);
	driver.register_function(test_init_from_string_is_boolean);
	driver.register_function(test_init_from_string_substr);
	driver.register_function(test_init_from_string_formattime);
	driver.register_function(test_init_from_string_strcmp);
	driver.register_function(test_init_from_string_attrnm);
	driver.register_function(test_init_from_string_regexp);
	driver.register_function(test_init_from_string_stringlists_regexpmember);
	driver.register_function(test_init_from_string_various);
	driver.register_function(test_int_invalid);
	driver.register_function(test_int_false);
	driver.register_function(test_int_true);
	driver.register_function(test_int_float_negative_quotes);
	driver.register_function(test_int_float_negative);
	driver.register_function(test_int_float_positive);
	driver.register_function(test_int_int_positive);
	driver.register_function(test_int_error);
	driver.register_function(test_real_invalid);
	driver.register_function(test_real_false);
	driver.register_function(test_real_true);
	driver.register_function(test_real_float_negative_quotes);
	driver.register_function(test_real_float_negative);
	driver.register_function(test_real_float_positive);
	driver.register_function(test_real_int_positive);
	driver.register_function(test_real_error);
	driver.register_function(test_if_then_else_false);
	driver.register_function(test_if_then_else_false_error);
	driver.register_function(test_if_then_else_false_constant);
	driver.register_function(test_if_then_else_true);
	driver.register_function(test_if_then_else_true_error);
	driver.register_function(test_if_then_else_true_constant1);
	driver.register_function(test_if_then_else_true_constant2);
	driver.register_function(test_if_then_else_invalid1);
	driver.register_function(test_if_then_else_invalid2);
	driver.register_function(test_if_then_else_too_few);
	driver.register_function(test_if_then_else_undefined);
	driver.register_function(test_string_list_size_3);
	driver.register_function(test_string_list_size_0);
	driver.register_function(test_string_list_size_5);
	driver.register_function(test_string_list_size_default_delim);
	driver.register_function(test_string_list_size_repeat_delim);
	driver.register_function(test_string_list_size_non_default_delim);
	driver.register_function(test_string_list_size_two_delim);
	driver.register_function(test_string_list_size_three_delim);
	driver.register_function(test_string_list_size_error_end);
	driver.register_function(test_string_list_size_error_beginning);
	driver.register_function(test_string_list_sum_default);
	driver.register_function(test_string_list_sum_empty);
	driver.register_function(test_string_list_sum_non_default);
	driver.register_function(test_string_list_sum_both);
	driver.register_function(test_string_list_sum_error_end);
	driver.register_function(test_string_list_sum_error_beginning);
	driver.register_function(test_string_list_sum_error_all);
	driver.register_function(test_string_list_min_negative);
	driver.register_function(test_string_list_min_positive);
	driver.register_function(test_string_list_min_both);
	driver.register_function(test_string_list_min_undefined);
	driver.register_function(test_string_list_min_error_end);
	driver.register_function(test_string_list_min_error_beginning);
	driver.register_function(test_string_list_min_error_all);
	driver.register_function(test_string_list_min_error_middle);
	driver.register_function(test_string_list_max_negatve);
	driver.register_function(test_string_list_max_positive);
	driver.register_function(test_string_list_max_both);
	driver.register_function(test_string_list_max_undefined);
	driver.register_function(test_string_list_max_error_end);
	driver.register_function(test_string_list_max_error_beginning);
	driver.register_function(test_string_list_max_error_all);
	driver.register_function(test_string_list_max_error_middle);
	driver.register_function(test_string_list_avg_default);
	driver.register_function(test_string_list_avg_empty);
	driver.register_function(test_string_list_avg_non_default);
	driver.register_function(test_string_list_avg_both);
	driver.register_function(test_string_list_avg_error_end);
	driver.register_function(test_string_list_avg_error_beginning);
	driver.register_function(test_string_list_avg_error_all);
	driver.register_function(test_string_list_avg_error_middle);
	driver.register_function(test_string_list_member);
	driver.register_function(test_string_list_member_case);
	driver.register_function(test_string_negative_int);
	driver.register_function(test_string_positive_int);
	driver.register_function(test_strcat_short);
	driver.register_function(test_strcat_long);
	driver.register_function(test_floor_negative_int);
	driver.register_function(test_floor_negative_float);
	driver.register_function(test_floor_positive_integer);
	driver.register_function(test_floor_positive_integer_wo);
	driver.register_function(test_floor_positive_float_wo);
	driver.register_function(test_ceiling_negative_int);
	driver.register_function(test_ceiling_negative_float);
	driver.register_function(test_ceiling_positive_integer);
	driver.register_function(test_ceiling_positive_integer_wo);
	driver.register_function(test_ceiling_positive_float_wo);
	driver.register_function(test_round_negative_int);
	driver.register_function(test_round_negative_float);
	driver.register_function(test_round_positive_int);
	driver.register_function(test_round_positive_float_wo_up);
	driver.register_function(test_round_positive_float_wo_down);
	driver.register_function(test_random_integer);
	driver.register_function(test_random);
	driver.register_function(test_random_float);
	driver.register_function(test_is_string_simple);
	driver.register_function(test_is_string_concat);
	driver.register_function(test_is_undefined_true);
	driver.register_function(test_is_undefined_false);
	driver.register_function(test_is_error_random);
	driver.register_function(test_is_error_int);
	driver.register_function(test_is_error_real);
	driver.register_function(test_is_error_floor);
	driver.register_function(test_is_integer_false_negative);
	driver.register_function(test_is_integer_false_positive);
	driver.register_function(test_is_integer_false_quotes);
	driver.register_function(test_is_integer_true_negative);
	driver.register_function(test_is_integer_true_int);
	driver.register_function(test_is_integer_true_int_quotes);
	driver.register_function(test_is_integer_true_positive);
	driver.register_function(test_is_real_false_negative_int);
	driver.register_function(test_is_real_false_negative_int_quotes);
	driver.register_function(test_is_real_false_positive_int);
	driver.register_function(test_is_real_true_negative);
	driver.register_function(test_is_real_true_positive);
	driver.register_function(test_is_real_true_real);
	driver.register_function(test_is_real_true_real_quotes);
	driver.register_function(test_is_real_error);
	driver.register_function(test_is_boolean);
	driver.register_function(test_substr_end);
	driver.register_function(test_substr_middle);
	driver.register_function(test_substr_negative_index);
	driver.register_function(test_substr_negative_length);
	driver.register_function(test_substr_out_of_bounds);
	driver.register_function(test_substr_error_index);
	driver.register_function(test_substr_error_string);
	driver.register_function(test_formattime_empty);
	driver.register_function(test_formattime_current);
	driver.register_function(test_formattime_current_options);
	driver.register_function(test_formattime_int);
	driver.register_function(test_formattime_error);
	driver.register_function(test_strcmp_positive);
	driver.register_function(test_strcmp_negative);
	driver.register_function(test_strcmp_equal);
	driver.register_function(test_strcmp_convert1);
	driver.register_function(test_strcmp_convert2);
	driver.register_function(test_stricmp_positive);
	driver.register_function(test_stricmp_negative);
	driver.register_function(test_stricmp_equal);
	driver.register_function(test_stricmp_convert1);
	driver.register_function(test_stricmp_convert2);
	driver.register_function(test_regexp_match_wildcard);
	driver.register_function(test_regexp_match_repeat);
	driver.register_function(test_regexp_no_match);
	driver.register_function(test_regexp_no_match_case);
	driver.register_function(test_regexp_error_pattern);
	driver.register_function(test_regexp_error_target);
	driver.register_function(test_regexp_error_option);
	driver.register_function(test_regexps_match);
	driver.register_function(test_regexps_match_case);
	driver.register_function(test_regexps_error_pattern);
	driver.register_function(test_regexps_error_target);
	driver.register_function(test_regexps_error_option);
	driver.register_function(test_regexps_error_return);
	driver.register_function(test_regexps_full);
	driver.register_function(test_regexps_full_global);
	driver.register_function(test_replace);
	driver.register_function(test_replaceall);
	driver.register_function(test_stringlist_regexp_member_match_default);
	driver.register_function(test_stringlist_regexp_member_match_non_default);
	driver.register_function(test_stringlist_regexp_member_match_case);
	driver.register_function(test_stringlist_regexp_member_match_repeat);
	driver.register_function(test_stringlist_regexp_member_no_match_multiple);
	driver.register_function(test_stringlist_regexp_member_no_match);
	driver.register_function(test_stringlist_regexp_member_no_match_case);
	driver.register_function(test_stringlist_regexp_member_error_pattern);
	driver.register_function(test_stringlist_regexp_member_error_target);
	driver.register_function(test_stringlist_regexp_member_error_delim);
	driver.register_function(test_stringlist_regexp_member_error_option);
	driver.register_function(test_random_different);
	driver.register_function(test_random_range);
	driver.register_function(test_equality);
	driver.register_function(test_inequality);
	driver.register_function(test_operators_short_or);
	driver.register_function(test_operators_no_short_or);
	driver.register_function(test_operators_short_and);
	driver.register_function(test_operators_short_and_error);
	driver.register_function(test_operators_no_short_and);
	driver.register_function(test_scoping_my);
	driver.register_function(test_scoping_my_dup);
	driver.register_function(test_scoping_target);
	driver.register_function(test_scoping_target_dup);
	driver.register_function(test_scoping_both);
	driver.register_function(test_scoping_my_miss);
	driver.register_function(test_scoping_target_miss);
	driver.register_function(test_time);
	driver.register_function(test_interval_minute);
	driver.register_function(test_interval_hour);
	driver.register_function(test_interval_day);
	driver.register_function(test_to_upper);
	driver.register_function(test_to_lower);
	driver.register_function(test_size_positive);
	driver.register_function(test_size_zero);
	driver.register_function(test_size_undefined);
	driver.register_function(test_nested_ads);

	return driver.do_all_functions();
}

static bool test_copy_constructor_actuals() {
	emit_test("Test that a copied ClassAd has the same attributes and actuals"
		" as the original ClassAd.");
	const char* classad_string = "\tA=1\n\t\tB=TRUE\n\t\tC=\"String\"";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	ClassAd* classadCopy = new ClassAd(*classad);
	int actual1 = -1;
	bool actual2 = false;
	std::string actual3;
	classadCopy->LookupInteger("A", actual1);
	classadCopy->LookupBool("B", actual2);
	classadCopy->LookupString("C", actual3);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_param("A", "1");
	emit_param("B", "true");
	emit_param("C", "String");
	emit_output_actual_header();
	emit_param("A", "%d", actual1);
	emit_param("B", "%s", actual2?"true":"false");
	emit_param("C", "%s", actual3.c_str());
	if(actual1 != 1 || actual2 != true || strcmp(actual3.c_str(), "String") != MATCH) {
		delete classad; delete classadCopy;
		FAIL;
	}
	delete classad; delete classadCopy;
	PASS;
}

static bool test_copy_constructor_pointer() {
	emit_test("Test that a copied ClassAd pointer is not equal to the "
		"original ClassAd pointer.");
	const char* classad_string = "\tA=1\n\t\tB=TRUE\n\t\tC=\"String\"";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	ClassAd* classadCopy = new ClassAd(*classad);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_param("ClassAd Copy", "!= %p", classad);
	emit_output_actual_header();
	emit_param("ClassAd Copy", "%p", classadCopy);
	if(classad == classadCopy) {
		delete classad; delete classadCopy;
		FAIL;
	}
	delete classad; delete classadCopy;
	PASS;
}

static bool test_assignment_actuals() {
	emit_test("Test that a ClassAd has the same attributes and actuals as the"
		" original ClassAd that it was assigned to.");
	const char* classad_string = "\tA=1\n\t\tB=TRUE\n\t\tC=\"String\"";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	ClassAd* classadAssign = new ClassAd;
	*classadAssign = *classad;
	int actual1 = -1;
	bool actual2 = false;
	std::string actual3;
	classadAssign->LookupInteger("A", actual1);
	classadAssign->LookupBool("B", actual2);
	classadAssign->LookupString("C", actual3);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_param("A", "1");
	emit_param("B", "true");
	emit_param("C", "String");
	emit_output_actual_header();
	emit_param("A", "%d", actual1);
	emit_param("B", "%s", actual2?"true":"false");
	emit_param("C", "%s", actual3.c_str());
	if(actual1 != 1 || actual2 != true || strcmp(actual3.c_str(), "String") != MATCH) {
		delete classad; delete classadAssign;
		FAIL;
	}
	delete classad; delete classadAssign;
	PASS;
}

static bool test_assignment_actuals_before() {
	emit_test("Test that a ClassAd has the same attributes and actuals as the"
		" original ClassAd that it was assigned to when the ClassAd had actuals"
		" beforehand.");
	const char* classad_string = "\tA=1\n\t\tB=TRUE\n\t\tC=\"String\"";
	const char* classad_string2  = "\tA=0\n\t\tB=FALSE\n\t\tC=\"gnirtS\"";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	ClassAd* classadAssign = new ClassAd;
	initAdFromString(classad_string2, *classadAssign);
	*classadAssign = *classad;
	int actual1 = -1;
	bool actual2 = false;
	std::string actual3;
	classadAssign->LookupInteger("A", actual1);
	classadAssign->LookupBool("B", actual2);
	classadAssign->LookupString("C", actual3);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_param("A", "1");
	emit_param("B", "true");
	emit_param("C", "String");
	emit_output_actual_header();
	emit_param("A", "%d", actual1);
	emit_param("B", "%s", actual2?"true":"false");
	emit_param("C", "%s", actual3.c_str());
	if(actual1 != 1 || actual2 != true || strcmp(actual3.c_str(), "String") != MATCH) {
		delete classad; delete classadAssign;
		FAIL;
	}
	delete classad; delete classadAssign;
	PASS;
}

static bool test_assignment_pointer() {
	emit_test("Test that a ClassAd pointer is not equal to the original "
		"ClassAd pointer that it was assigned to.");
	const char* classad_string = "\tA=1\n\t\tB=TRUE\n\t\tC=\"String\"";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	ClassAd* classadAssign = new ClassAd;
	*classadAssign = *classad;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_param("Assigned ClassAd", "!= %p", classad);
	emit_output_actual_header();
	emit_param("Assigned ClassAd", "%p", classadAssign);
	if(classad == classadAssign) {
		delete classad; delete classadAssign;
		FAIL;
	}
	delete classad; delete classadAssign;
	PASS;
}

static bool test_xml() {
	emit_test("Test that a ClassAd is the same after printing to a string, "
		"converting to XML, and back.");
	emit_comment("This doesn't look like it preserves tabs.");
	const char* classad_string = "\tA=1\n\t\tB=TRUE\n\t\tC=\"String\"\n\t\t"
		"D=\"\"\n\t\tE=\" \"";
	classad::ClassAdXMLUnParser unparser;
	classad::ClassAdXMLParser parser;
	ClassAd classad, *classadAfter;
	MyString before, after;
	std::string xml;
	initAdFromString(classad_string, classad);
	sPrintAd(before, classad);
	unparser.SetCompactSpacing(false);
	unparser.Unparse(xml, &classad);
	classadAfter = new ClassAd();
	parser.ParseClassAd(xml, *classadAfter);
	sPrintAd(after, *classadAfter);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_param("Before", classad_string);
	emit_param("After", classad_string);
	emit_output_actual_header();
	emit_param("Before", before.Value());
	emit_param("After", after.Value());
	if(!classad.SameAs(classadAfter)) {
		delete classadAfter;
		FAIL;
	}
	delete classadAfter;
	PASS;
}

static bool test_lookup_integer_first() {
	emit_test("Test LookupInteger() on the first attribute in a classad.");
	const char* classad_string = "\tA = 1\n\t\tB = 2\n\t\tC = 3";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "A";
	int actual = -1;
	classad.LookupInteger(attribute_name, actual);
	int expect = 1;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_param("INT", "%d", expect);
	emit_output_actual_header();
	emit_param("INT", "%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_integer_middle() {
	emit_test("Test LookupInteger() on the second attribute in a classad.");
	const char* classad_string = "\tA = 1\n\t\tB = 2\n\t\tC = 3";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "B";
	int actual = -1;
	classad.LookupInteger(attribute_name, actual);
	int expect = 2;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_param("INT", "%d", expect);
	emit_output_actual_header();
	emit_param("INT", "%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_integer_last() {
	emit_test("Test LookupInteger() on the last attribute in a classad.");
	const char* classad_string = "\tA = 1\n\t\tB = 2\n\t\tC = 3";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "C";
	int actual = -1;
	classad.LookupInteger(attribute_name, actual);
	int expect = 3;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_param("INT", "%d", expect);
	emit_output_actual_header();
	emit_param("INT", "%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_bool_true() {
	emit_test("Test LookupBool() on an attribute in a classad that evaluates to"
		" true.");
	const char* classad_string = "\tDoesMatch = \"Bone Machine\" == \"bone "
		"machine\" && \"a\" =?= \"a\" && \"a\" =!= \"A\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "DoesMatch";
	bool actual = false;
	classad.LookupBool(attribute_name, actual);
	bool expect = true;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_bool_false() {
	emit_test("Test LookupBool() on an attribute in a classad that evaluates to"
		" false.");
	const char* classad_string = "\tDoesntMatch = \"a\" =?= \"A\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "DoesntMatch";
	bool actual = true;
	classad.LookupBool(attribute_name, actual);
	bool expect = false;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_expr_error_or_false() {
	emit_test("Test LookupExpr() on an attribute in a classad that contains "
		"an error when used in a logical OR with a FALSE.");
	const char* classad_string = "\tE = FALSE || ERROR";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "E";
	ExprTree * tree = classad.LookupExpr(attribute_name);
	classad::Value val;
	int actual1 = EvalExprTree(tree, &classad, NULL, val);
	int actual2 = (val.GetType() == classad::Value::ERROR_VALUE);
	int expect = 1;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_output_expected_header();
	emit_param("EvalExprTree", "%d", expect);
	emit_param("LX_ERROR", "%d", expect);
	emit_output_actual_header();
	emit_param("EvalExprTree", "%d", actual1);
	emit_param("LX_ERROR", "%d", actual2);
	if(actual1 != expect || actual2 != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_expr_error_and() {
	emit_test("Test LookupExpr() on an attribute in a classad that contains "
		"an error when used in a logical AND.");
	const char* classad_string = "\tL = \"foo\" && ERROR";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "L";
	ExprTree * tree = classad.LookupExpr(attribute_name);
	classad::Value val;
	int actual1 = EvalExprTree(tree, &classad, NULL, val);
	int actual2 = (val.GetType() == classad::Value::ERROR_VALUE);
	int expect = 1;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_output_expected_header();
	emit_param("EvalExprTree", "%d", expect);
	emit_param("LX_ERROR", "%d", expect);
	emit_output_actual_header();
	emit_param("EvalExprTree", "%d", actual1);
	emit_param("LX_ERROR", "%d", actual2);
	if(actual1 != expect || actual2 != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_expr_error_and_true() {
	emit_test("Test LookupExpr() on an attribute in a classad that contains "
		"an error when used in a logical AND with a TRUE.");
	const char* classad_string = "\tM = TRUE && ERROR";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "M";
	ExprTree * tree = classad.LookupExpr(attribute_name);
	classad::Value val;
	int actual1 = EvalExprTree(tree, &classad, NULL, val);
	int actual2 = (val.GetType() == classad::Value::ERROR_VALUE);
	int expect = 1;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_output_expected_header();
	emit_param("EvalExprTree", "%d", expect);
	emit_param("LX_ERROR", "%d", expect);
	emit_output_actual_header();
	emit_param("EvalExprTree", "%d", actual1);
	emit_param("LX_ERROR", "%d", actual2);
	if(actual1 != expect || actual2 != expect) {
		FAIL;
	}
	PASS;
}

static bool test_lookup_string_normal() {
	emit_test("Test LookupString() on an attribute in a classad that contains"
		" a typical attribute name and expression.");
	const char* classad_string = "\tD = \"alain\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "D";
	int expectInt = 1;
	const char* expectString = "alain";
	char* result = NULL;
	int found = classad.LookupString(attribute_name, &result);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("%d", expectInt);
	emit_param("STRING", "%s", expectString);
	emit_output_actual_header();
	emit_retval("%d", found);
	emit_param("STRING", "%s", result);
	if(found != expectInt || strcmp(result, expectString) != MATCH) {
		free(result);
		FAIL;
	}
	free(result);
	PASS;
}

static bool test_lookup_string_long() {
	emit_test("Test LookupString() on an attribute in a classad that has a "
		" very long name and expression.");
	emit_comment("This test requires the use of Insert() so if there is a "
		"problem with that, this test may fail.");
	emit_comment("The attribute name and string are not printed here due to "
		"the large size of the strings.");
	char *attribute_name, *expectString;
	make_big_string(15000, &attribute_name, NULL);
	make_big_string(25000, &expectString, NULL);
	ClassAd classad;
	classad.Assign(attribute_name, expectString);
	int expectInt = 1;
	char* result = NULL;
	int found = classad.LookupString(attribute_name, &result);
	emit_input_header();
	emit_param("ClassAd", "");
	emit_param("Attribute", "");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("%d", expectInt);
	emit_param("STRING", "");
	emit_output_actual_header();
	emit_retval("%d", found);
	emit_param("STRING", "");
	if(found != expectInt || strcmp(result, expectString) != MATCH) {
		free(attribute_name); free(expectString); 
		free(result);
		FAIL;
	}
	free(attribute_name); free(expectString); 
	free(result);
	PASS;
}

static bool test_lookup_string_file() {
	emit_test("Test LookupString() on an attribute in a classad that was read"
		" from a file.");
	ClassAd* classad = get_classad_from_file();
	const char* attribute_name = "D";
	int expectInt = 1;
	const char* expectString = "alain";
	char* result = NULL;
	int found = classad->LookupString(attribute_name, &result);
	emit_input_header();
	emit_param("Attribute", attribute_name);
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("%d", expectInt);
	emit_param("STRING", "%s", expectString);
	emit_output_actual_header();
	emit_retval("%d", found);
	emit_param("STRING", "%s", result);
	if(found != expectInt || strcmp(result, expectString) != MATCH) {
		delete classad;	
		free(result);	
		FAIL;
	}
	delete classad; free(result);
	PASS;
}

static bool test_get_my_type_name_no() {
	emit_test("Test GetMyTypeName() on a classad that doesn't have a type "
		"name.");
	const char* classad_string = "\tA = 1\n\t\tB=2\n\t\tC = 3\n\t\t"
		"D='2001-04-05T12:14:15'\n\t\tG=GetTime(1)\n\t\tH=foo(1)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* expect = "";
	const char* result = GetMyTypeName(classad);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", result);
	if(strcmp(result, expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_my_type_name_yes() {
	emit_test("Test GetMyTypeName() on a classad that has a type name.");
	const char* classad_string = "\tA = 0.7\n\t\tB=2\n\t\tC = 3\n\t\tD = \"alain\""
		"\n\t\tMyType=\"foo\"\n\t\tTargetType=\"blah\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* expect = "foo";
	const char* result = GetMyTypeName(classad);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", result);
	if(strcmp(result, expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_target_type_name_no() {
	emit_test("Test GetTargetTypeName() on a classad that doesn't have a "
		"target type name.");
	const char* classad_string = "\tA = 1\n\t\tB=2\n\t\tC = 3\n\t\t"
		"D='2001-04-05T12:14:15'\n\t\tG=GetTime(1)\n\t\tH=foo(1)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* expect = "";
	const char* result = GetTargetTypeName(classad);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", result);
	if(strcmp(result, expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_target_type_name_yes() {
	emit_test("Test GetTargetTypeName() on a classad that has a target type"
		" name.");
	const char* classad_string = "\tA = 0.7\n\t\tB=2\n\t\tC = 3\n\t\t"
		"D = \"alain\"\n\t\tMyType=\"foo\"\n\t\tTargetType=\"blah\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* expect = "blah";
	const char* result = GetTargetTypeName(classad);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", result);
	if(strcmp(result, expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_is_a_match_true() {
	emit_test("Test that IsAMatch() returns true for two classads that "
		"match.");
    const char* classad_string1 = "\tMyType=\"Job\"\n\t\tTargetType=\"Machine\"\n\t\t"
		"Owner = \"alain\"\n\t\tRequirements = (TARGET.Memory > 50)";
    const char* classad_string2 = "\tMyType=\"Machine\"\n\t\tTargetType=\"Job\"\n\t\t"
		"Memory = 100\n\t\tRequirements = (TARGET.owner == \"alain\")";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	bool expect = true;
	bool result = IsAMatch(&classad1, &classad2);
	emit_input_header();
	emit_param("ClassAd1", classad_string1);
	emit_param("ClassAd2", classad_string2);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_a_match_true_reverse() {
	emit_test("Test that IsAMatch() returns true for two classads that "
		"match with the arguments in reverse order.");
    const char* classad_string1 = "\tMyType=\"Job\"\n\t\tTargetType=\"Machine\"\n\t\t"
		"Owner = \"alain\"\n\t\tRequirements = (TARGET.Memory > 50)";
    const char* classad_string2 = "\tMyType=\"Machine\"\n\t\tTargetType=\"Job\"\n\t\t"
		"Memory = 100\n\t\tRequirements = (TARGET.owner == \"alain\")";
	ClassAd classad1, classad2;
	initAdFromString(classad_string2, classad1);
	initAdFromString(classad_string1, classad2);
	bool expect = true;
	bool result = IsAMatch(&classad1, &classad2);
	emit_input_header();
	emit_param("ClassAd1", classad_string2);
	emit_param("ClassAd2", classad_string1);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_a_match_false_memory() {
	emit_test("Test that IsAMatch() returns false for two classads that "
		"don't match due to a memory attribute difference.");
    const char* classad_string1 = "\tMyType=\"Job\"\n\t\tTargetType=\"Machine\"\n\t\t"
		"Owner = \"alain\"\n\t\tRequirements = (TARGET.Memory > 50)";
    const char* classad_string2 = "\tMyType=\"Machine\"\n\t\tTargetType=\"Job\"\n\t\t"
		"Memory = 40\n\t\tRequirements = (TARGET.owner == \"alain\")";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	bool expect = false;
	bool result = IsAMatch(&classad1, &classad2);
	emit_input_header();
	emit_param("ClassAd1", classad_string1);
	emit_param("ClassAd2", classad_string2);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_a_match_false_memory_reverse() {
	emit_test("Test that IsAMatch() returns false for two classads that "
		"don't match due to a memory attribute difference with the arguments "
		"in reverse order.");
    const char* classad_string1 = "\tMyType=\"Job\"\n\t\tTargetType=\"Machine\"\n\t\t"
		"Owner = \"alain\"\n\t\tRequirements = (TARGET.Memory > 50)";
    const char* classad_string2 = "\tMyType=\"Machine\"\n\t\tTargetType=\"Job\"\n\t\t"
		"Memory = 40\n\t\tRequirements = (TARGET.owner == \"alain\")";
	ClassAd classad1, classad2;
	initAdFromString(classad_string2, classad1);
	initAdFromString(classad_string1, classad2);
	bool expect = false;
	bool result = IsAMatch(&classad1, &classad2);
	emit_input_header();
	emit_param("ClassAd1", classad_string1);
	emit_param("ClassAd2", classad_string2);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_a_match_false_owner() {
	emit_test("Test that IsAMatch() returns false for two classads that "
		"don't match due to a owner requirement difference.");
    const char* classad_string1 = "\tMyType=\"Job\"\n\t\tTargetType=\"Machine\"\n\t\t"
		"Owner = \"alain\"\n\t\tRequirements = (TARGET.Memory > 50)";
    const char* classad_string2 = "\tMyType=\"Machine\"\n\t\tTargetType=\"Job\"\n\t\t"
		"Memory = 100\n\t\tRequirements = (TARGET.owner != \"alain\")";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	bool expect = false;
	bool result = IsAMatch(&classad1, &classad2);
	emit_input_header();
	emit_param("ClassAd1", classad_string1);
	emit_param("ClassAd2", classad_string2);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_a_match_false_owner_reverse() {
	emit_test("Test that IsAMatch() returns false for two classads that "
		"don't match due to a owner requirement difference with the arguments "
		"in reverse order.");
    const char* classad_string1 = "\tMyType=\"Job\"\n\t\tTargetType=\"Machine\"\n\t\t"
		"Owner = \"alain\"\n\t\tRequirements = (TARGET.Memory > 50)";
    const char* classad_string2 = "\tMyType=\"Machine\"\n\t\tTargetType=\"Job\"\n\t\t"
		"Memory = 100\n\t\tRequirements = (TARGET.owner != \"alain\")";
	ClassAd classad1, classad2;
	initAdFromString(classad_string2, classad1);
	initAdFromString(classad_string1, classad2);
	bool expect = false;
	bool result = IsAMatch(&classad1, &classad2);
	emit_input_header();
	emit_param("ClassAd1", classad_string1);
	emit_param("ClassAd2", classad_string2);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_expr_tree_to_string_short() {
	emit_test("Test that ExprTreeToString() returns the correct string "
		"representation of the ExprTree of the attribute in the classad when "
		"the string is short.");
    const char* classad_string = "\tRank = (Memory >= 50)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "Rank";
	ExprTree* expr = classad.LookupExpr(attribute_name);
	const char* expect = "(Memory >= 50)";
	const char* result = ExprTreeToString(expr);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", result);
	if(strcmp(result, expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_expr_tree_to_string_long() {
	emit_test("Test that ExprTreeToString() returns the correct string "
		"representation of the ExprTree of the attribute in the classad when "
		"the string is long.");
    const char* classad_string = "\tEnv = \"CPUTYPE=i86pc;GROUP=unknown;"
		"LM_LICENSE_FILE=/p/multifacet/projects/simics/dist10/v9-sol7-gcc/sys/"
		"flexlm/license.dat;SIMICS_HOME=.;SIMICS_EXTRA_LIB=./modules;"
		"PYTHONPATH=./modules;MACHTYPE=i386;SHELL=/bin/tcsh;"
		"PATH=/s/std/bin:/usr/afsws/bin:/usr/ccs/bin:/usr/ucb:/bin:/usr/bin:/"
		"usr/X11R6/bin:/unsup/condor/bin:.\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "Env";
	ExprTree* expr = classad.LookupExpr(attribute_name);
	const char* expect =  "\"CPUTYPE=i86pc;GROUP=unknown;"
		"LM_LICENSE_FILE=/p/multifacet/projects/simics/dist10/v9-sol7-gcc/sys/"
		"flexlm/license.dat;SIMICS_HOME=.;SIMICS_EXTRA_LIB=./modules;"
		"PYTHONPATH=./modules;MACHTYPE=i386;SHELL=/bin/tcsh;"
		"PATH=/s/std/bin:/usr/afsws/bin:/usr/ccs/bin:/usr/ucb:/bin:/usr/bin:/"
		"usr/X11R6/bin:/unsup/condor/bin:.\"";
	const char* result = ExprTreeToString(expr);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", result);
	if(strcmp(result, expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_expr_tree_to_string_long2() {
	emit_test("Test that ExprTreeToString() returns the correct string "
		"representation of the ExprTree of the attribute in the classad when "
		"the string is long with many operators.");
    const char* classad_string = "\tRequirements = (a > 3) && (b >= 1.3) && "
		"(c < MY.rank) && ((d <= TARGET.RANK) || (g == \"alain\") || "
		"(g != \"roy\") || (h =?= 5) || (i =!= 6)) && ((a + b) < (c-d)) && "
		"((e * false) > (g / h)) && x == false && y == true && z == false && "
		"j == true";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	const char* attribute_name = "Requirements";
	ExprTree* expr = classad.LookupExpr(attribute_name);
    const char* expect = "(a > 3) && (b >= 1.3) && "
		"(c < MY.rank) && ((d <= TARGET.RANK) || (g == \"alain\") || "
		"(g != \"roy\") || (h =?= 5) || (i =!= 6)) && "
		"((a + b) < (c - d)) && ((e * false) > (g / h)) && "
		"x == false && y == true && z == false && j == true";
	const char* result = ExprTreeToString(expr);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", attribute_name);
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", result);
	if(strcmp(result, expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_expr_tree_to_string_big() {
	emit_test("Test that ExprTreeToString() returns the correct string "
		"representation of the ExprTree of the attribute in a very big "
		"classad.");
	emit_comment("The attribute name and string are not printed here due to "
		"the large size of the strings.");
	char* expect = (char *) malloc(25000 + 2 + 1);
	if ( ! expect) { FAIL; }
	char* attribute_name, *expectString;
	make_big_string(15000, &attribute_name, NULL);
	make_big_string(25000, &expectString, NULL);
	sprintf(expect, "\"%s\"", expectString);
	ClassAd classad;
	classad.Assign(attribute_name, expectString);
	ExprTree* expr = classad.LookupExpr(attribute_name);
	const char* result = ExprTreeToString(expr);
	emit_input_header();
	emit_param("ClassAd", "");
	emit_param("Attribute", "");
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", "");
	if(strcmp(result, expect) != MATCH) {
		free(attribute_name); free(expectString);
		free(expect);
		FAIL;
	}
	free(attribute_name); free(expectString);
	free(expect);
	PASS;
}

static bool test_get_references_simple_true_internal() {
	emit_test("Test that GetReferences() puts the references of the classad "
		"into the set for internal references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux"
		"\n\t\tX = 4\n\t\tRequirements = ((ImageSize > Memory) && "
		"(AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > "
		"Disk)) && foo(X, XX)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = true;
	bool result = internal_references.count("Memory") &&
		internal_references.count("Disk");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "Memory, Disk");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_references_simple_true_external() {
	emit_test("Test that GetReferences() puts the references of the classad "
		"into the set for external references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux"
		"\n\t\tX = 4\n\t\tRequirements = ((ImageSize > Memory) && "
		"(AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > "
		"Disk)) && foo(X, XX)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = true;
	bool result = external_references.count("ImageSize") &&
		external_references.count("AvailableDisk") &&
		external_references.count("XX");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "ImageSize, AvailableDisk, XX");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_references_simple_false_internal() {
	emit_test("Test that GetReferences() doesn't put the references of the "
		"classad into the incorrect references set for internal "
		"references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux"
		"\n\t\tX = 4\n\t\tRequirements = ((ImageSize > Memory) && "
		"(AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > "
		"Disk)) && foo(X, XX)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = false;
	bool result = internal_references.count("ImageSize") &&
		internal_references.count("AvailableDisk") &&
		internal_references.count("Linux");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "ImageSize, AvailableDisk, XX");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_references_simple_false_external() {
	emit_test("Test that GetReferences() doesn't put the references of the "
		"classad into the incorrect references set for external "
		"references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux"
		"\n\t\tX = 4\n\t\tRequirements = ((ImageSize > Memory) && "
		"(AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > "
		"Disk)) && foo(X, XX)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = false;
	bool result = external_references.count("Memory") &&
		external_references.count("Disk") &&
		external_references.count("Linux") &&
		external_references.count("X");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "Memory, Disk, Linux, X");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_references_complex_true_internal() {
	emit_test("Test that GetReferences() puts the references of the classad "
		"into the set for internal references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux"
		"\n\t\tX = 4\n\t\tFoo = Bar\n\t\tBar = True\n\t\tRequirements = ((ImageSize > Memory) && "
		"(AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > "
		"Disk)) && func(X, XX) && My.Foo";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = true;
	bool result = internal_references.count("Memory") &&
		internal_references.count("Disk") &&
		internal_references.count("Foo") &&
		internal_references.count("Bar") &&
		internal_references.count("X") &&
		internal_references.size() == 5;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "Memory, Disk, X, Foo, Bar");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_references_complex_true_external() {
	emit_test("Test that GetReferences() puts the references of the classad "
		"into the set for external references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux\n\t\t"
		"Requirements = ((TARGET.ImageSize > Memory) && (AvailableDisk > Disk) "
		"&& (TARGET.AvailableDisk > Memory) && (TARGET.ImageSize > Disk)) "
	    "&& foo(TARGET.X, TARGET.XX)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = true;
	bool result = external_references.count("ImageSize") &&
		external_references.count("AvailableDisk") &&
		external_references.count("X") &&
		external_references.count("XX");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "ImageSize, AvailableDisk, X, XX");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_references_complex_false_internal() {
	emit_test("Test that GetReferences() doesn't put the references of the "
		"classad into the incorrect references set for internal "
		"references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux"
		"\n\t\tX = 4\n\t\tRequirements = ((ImageSize > Memory) && "
		"(AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > "
		"Disk)) && foo(X, XX)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = false;
	bool result = internal_references.count("ImageSize") &&
		internal_references.count("AvailableDisk") &&
		internal_references.count("Linux");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "ImageSize, AvailableDisk, Linux");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_references_complex_false_external() {
	emit_test("Test that GetReferences() doesn't put the references of the "
		"classad into the incorrect references set for external "
		"references.");
    const char* classad_string = "\tMemory = 60\n\t\tDisk = 40\n\t\tOS = Linux"
		"\n\t\tX = 4\n\t\tRequirements = ((ImageSize > Memory) && "
		"(AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > "
		"Disk)) && foo(X, XX)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad::References internal_references;
	classad::References external_references;
	GetReferences("Requirements", classad, &internal_references,
		&external_references);
	bool expect = false;
	bool result = external_references.count("Memory") &&
		external_references.count("Disk") &&
		external_references.count("Linux");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "Requirements");
	emit_param("StringList", "Internal References");
	emit_param("StringList", "External References");
	emit_param("Contains", "Memory, Disk, Linux");
	emit_output_expected_header();
	emit_param("Contains References", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("Contains References", "%s", tfstr(result));
	if(result != expect) {
		FAIL;
	}
	PASS;
}

static bool test_reference_name_trimming_internal() {
	emit_test("Test that TrimReferenceNames() edits the full names "
		"of internal references properly.");
	classad::References initial_set;
	classad::References result_set;
	classad::References expect_set;
	initial_set.insert("Name");
	initial_set.insert("TARGET.Rank");
	initial_set.insert(".Disk");
	initial_set.insert("Parent.Child");
	initial_set.insert(".left.Outer.Inner");
	result_set = initial_set;
	expect_set.insert("Name");
	expect_set.insert("TARGET");
	expect_set.insert("Disk");
	expect_set.insert("Parent");
	expect_set.insert("left");
	TrimReferenceNames(result_set, false);
	std::string initial_str;
	std::string expect_str;
	std::string result_str;
	classad::References::iterator it;
	for ( it = initial_set.begin(); it != initial_set.end(); it++ ) {
		initial_str += *it;
		initial_str += " ";
	}
	for ( it = expect_set.begin(); it != expect_set.end(); it++ ) {
		expect_str += *it;
		expect_str += " ";
	}
	for ( it = result_set.begin(); it != result_set.end(); it++ ) {
		result_str += *it;
		result_str += " ";
	}
	emit_input_header();
	emit_param("References", "%s", initial_str.c_str());
	emit_output_expected_header();
	emit_param("References", "%s", expect_str.c_str());
	emit_output_actual_header();
	emit_param("References", "%s", result_str.c_str());
	if(result_set != expect_set) {
		FAIL;
	}
	PASS;
}

static bool test_reference_name_trimming_external() {
	emit_test("Test that TrimReferenceNames() edits the full names "
		"of external references properly.");
	classad::References initial_set;
	classad::References result_set;
	classad::References expect_set;
	initial_set.insert("Name");
	initial_set.insert("TARGET.Rank");
	initial_set.insert(".Disk");
	initial_set.insert("Parent.Child");
	initial_set.insert(".left.Outer.Inner");
	result_set = initial_set;
	expect_set.insert("Name");
	expect_set.insert("Rank");
	expect_set.insert("Disk");
	expect_set.insert("Parent");
	expect_set.insert("Outer");
	TrimReferenceNames(result_set, true);
	std::string initial_str;
	std::string expect_str;
	std::string result_str;
	classad::References::iterator it;
	for ( it = initial_set.begin(); it != initial_set.end(); it++ ) {
		initial_str += *it;
		initial_str += " ";
	}
	for ( it = expect_set.begin(); it != expect_set.end(); it++ ) {
		expect_str += *it;
		expect_str += " ";
	}
	for ( it = result_set.begin(); it != result_set.end(); it++ ) {
		result_str += *it;
		result_str += " ";
	}
	emit_input_header();
	emit_param("References", "%s", initial_str.c_str());
	emit_output_expected_header();
	emit_param("References", "%s", expect_str.c_str());
	emit_output_actual_header();
	emit_param("References", "%s", result_str.c_str());
	if(result_set != expect_set) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_clear() {
	emit_test("Test that dirtyBegin() returns no dirty attributes after "
		"calling ClearAllDirtyFlags() on a classad.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	auto dirty_itr = classad.dirtyBegin();
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Has Dirty Attribute", "FALSE");
	emit_output_actual_header();
	emit_param("Has Dirty Attribute", "%s", tfstr(dirty_itr != classad.dirtyEnd()));
	if(dirty_itr != classad.dirtyEnd()) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_insert() {
	emit_test("Test that dirtyBegin() returns a dirty attribute after "
		"inserting an attribute into the classad.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.Assign("C", 3);
	auto dirty_itr = classad.dirtyBegin();
	const char* name = (dirty_itr == classad.dirtyEnd()) ? "" : dirty_itr->c_str();
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Dirty Attribute", "C");
	emit_output_actual_header();
	emit_param("Dirty Attribute", name);
	if(strcasecmp(name, "C") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_insert_two_calls() {
	emit_test("Test that dirtyBegin() returns no dirty attributes after "
		"inserting one attribute into the classad and incrementing the "
		"iterator one time.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.Assign("C", 3);
	auto dirty_itr = classad.dirtyBegin();
	dirty_itr++;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Has Dirty Attribute", "FALSE");
	emit_output_actual_header();
	emit_param("Has Dirty Attribute", "%s", tfstr(dirty_itr != classad.dirtyEnd()));
	if(dirty_itr != classad.dirtyEnd()) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_two_inserts_first() {
	emit_test("Test that dirtyBegin() returns a dirty attribute after "
		"inserting two attributes into the classad.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.Assign("C", 3);
	classad.Assign("D", 4);
	auto dirty_itr = classad.dirtyBegin();
	const char* name = (dirty_itr == classad.dirtyEnd()) ? "" : dirty_itr->c_str();
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Dirty Attribute", "C");
	emit_output_actual_header();
	emit_param("Dirty Attribute", name);
	if(strcasecmp(name, "C") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_two_inserts_second() {
	emit_test("Test that dirtyBegin() returns a dirty attribute after "
		"inserting two attributes into the classad and incrementing the "
		"iterator one time.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	classad.EnableDirtyTracking();
	initAdFromString(classad_string, classad);
	classad.ClearAllDirtyFlags();
	classad.Assign("C", 3);
	classad.Assign("D", 4);
	auto dirty_itr = classad.dirtyBegin();
	dirty_itr++;
	const char* name = (dirty_itr == classad.dirtyEnd()) ? "" : dirty_itr->c_str();
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Dirty Attribute", "D");
	emit_output_actual_header();
	emit_param("Dirty Attribute", name);
	if(strcasecmp(name, "D") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_two_inserts_third() {
	emit_test("Test that dirtyBegin() returns no dirty attributes after "
		"inserting two attributes into the classad and incrementing the "
		"iterator two times.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.Assign("C", 3);
	classad.Assign("D", 4);
	auto dirty_itr = classad.dirtyBegin();
	dirty_itr++;
	dirty_itr++;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Has Dirty Attribute", "FALSE");
	emit_output_actual_header();
	emit_param("Has Dirty Attribute", "%s", tfstr(dirty_itr != classad.dirtyEnd()));
	if(dirty_itr != classad.dirtyEnd()) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_two_inserts_clear() {
	emit_test("Test that dirtyBegin() returns no dirty attributes after "
		"inserting two attributes into the classad and then calling "
		"ClearAllDirtyFlags().");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.Assign("C", 3);
	classad.Assign("D", 4);
	classad.ClearAllDirtyFlags();
	auto dirty_itr = classad.dirtyBegin();
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Has Dirty Attribute", "FALSE");
	emit_output_actual_header();
	emit_param("Has Dirty Attribute", "%s", tfstr(dirty_itr != classad.dirtyEnd()));
	if(dirty_itr != classad.dirtyEnd()) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_set_first() {
	emit_test("Test that dirtyBegin() returns a dirty attribute after "
		"setting both attributes to dirty and then setting one to not dirty.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.MarkAttributeDirty("A");
	classad.MarkAttributeDirty("B");
	classad.MarkAttributeClean("B");
	auto dirty_itr = classad.dirtyBegin();
	const char* name = (dirty_itr == classad.dirtyEnd()) ? "" : dirty_itr->c_str();
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Dirty Attribute", "A");
	emit_output_actual_header();
	emit_param("Dirty Attribute", name);
	if(strcmp(name, "A") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_next_dirty_expr_set_second() {
	emit_test("Test that dirtyBegin() returns no dirty attributes after "
		"setting both attributes to dirty, setting one to not dirty, and then "
		"incrementing the iterator once.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.MarkAttributeDirty("A");
	classad.MarkAttributeDirty("B");
	classad.MarkAttributeClean("B");
	auto dirty_itr = classad.dirtyBegin();
	dirty_itr++;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("STRING", "");
	emit_param("ExprTree", "");
	emit_output_expected_header();
	emit_param("Has Dirty Attribute", "FALSE");
	emit_output_actual_header();
	emit_param("Has Dirty Attribute", "%s", tfstr(dirty_itr != classad.dirtyEnd()));
	if(dirty_itr != classad.dirtyEnd()) {
		FAIL;
	}
	PASS;
}

static bool test_get_dirty_flag_exists_dirty() {
	emit_test("Test that IsAttributeDirty() returns true "
		"when the given name exists and is dirty.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.MarkAttributeDirty("A");
	classad.MarkAttributeDirty("B");
	classad.MarkAttributeClean("B");
	bool dirty = classad.IsAttributeDirty("A");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "A");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("'A' is Dirty", "TRUE");
	emit_output_actual_header();
	emit_param("'A' is Dirty", tfstr(dirty));
	if(!dirty) {
		FAIL;
	}
	PASS;
}

static bool test_get_dirty_flag_exists_not_dirty() {
	emit_test("Test that IsAttributeDirty() returns false when the "
		"given name exists but is not dirty.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.MarkAttributeDirty("A");
	classad.MarkAttributeDirty("B");
	classad.MarkAttributeClean("B");
	bool dirty = classad.IsAttributeDirty("B");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("'B' is Dirty", "FALSE");
	emit_output_actual_header();
	emit_param("'B' is Dirty", tfstr(dirty));
	if(dirty) {
		FAIL;
	}
	PASS;
}

static bool test_get_dirty_flag_not_exist() {
	emit_test("Test that IsAttributeDirty() returns false when the given "
		"name doesn't exist.");
	const char* classad_string = "\tA = 1\n\t\tB = 2";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad.EnableDirtyTracking();
	classad.ClearAllDirtyFlags();
	classad.MarkAttributeDirty("A");
	classad.MarkAttributeDirty("B");
	classad.MarkAttributeClean("B");
	bool dirty = classad.IsAttributeDirty("Unknown");
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("'Unknown' is Dirty", "FALSE");
	emit_output_actual_header();
	emit_param("'Unknown' Exists", tfstr(dirty));
	if(dirty) {
		FAIL;
	}
	PASS;
}

static bool test_from_file() {
	emit_test("Test that reading a classad from a file works correctly.");
	ClassAd* classad = get_classad_from_file();
	int actual1 = -1, actual2 = -1;
	char* actual3 = NULL;
	bool retVal1 = classad->LookupInteger("B", actual1);
	bool retVal2 = classad->LookupInteger("C", actual2);
	bool retVal3 = classad->LookupString("D", &actual3);
	emit_input_header();
	emit_output_expected_header();
	emit_param("'B' Exists", "TRUE");
	emit_param("'B'", "%d", 2);
	emit_param("'C' Exists", "TRUE");
	emit_param("'C'", "%d", 3);
	emit_param("'D' Exists", "TRUE");
	emit_param("'D'", "%s", "alain");
	emit_output_actual_header();
	emit_param("'B' Exists", "%s", tfstr(retVal1));
	emit_param("'B'", "%d", actual1);
	emit_param("'C' Exists", "%s", tfstr(retVal2));
	emit_param("'C'", "%d", actual2);
	emit_param("'D' Exists", "%s", tfstr(retVal3));
	emit_param("'D'", "%s", actual3);
	if(!retVal1 || !retVal2 || !retVal3 || actual1 != 2 || actual2 != 3 || 
			strcmp(actual3, "alain") != MATCH) {
		delete classad; free(actual3);
		FAIL;
	}
	delete classad; free(actual3);
	PASS;
}

static bool test_init_from_string_ints() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using int().");
    const char* classad_string = 	"\tB0=int(-3)\n\t\t"
							"B1=int(3.4)\n\t\t"
							"B2=int(-3.4)\n\t\t"
							"B3=int(\"-3.4\")\n\t\t"
							"B4=int(true)\n\t\t"
							"B6=int(false)\n\t\t"
							"B8=int(\"this is not a number\")\n\t\t"
							"B9=isError(B8)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_reals() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using real().");
    const char* classad_string = 	"\tB0=real(-3)\n\t\t"
							"B1=real(3.4)\n\t\t"
							"B2=real(-3.4)\n\t\t"
							"B3=real(\"-3.4\")\n\t\t"
							"B4=real(true)\n\t\t"
							"B6=real(false)\n\t\t"
							"B8=real(\"this is not a number\")\n\t\t"
							"B9=isError(B8)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_if_then_else() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using ifThenElse().");
    const char* classad_string = 	"\tBB=2\n\t\t"
							"BC=10\n\t\t"
							"BB2=ifThenElse(BB > 5, \"big\",\"small\")\n\t\t"
							"BB3=ifThenElse(BC > 5, \"big\",\"small\")\n\t\t"
							"BB4=ifThenElse(BD > 5, \"big\",\"small\")\n\t\t"
							"BB5=isUndefined(BB4)\n\t\t"
							"BB6=ifThenElse(4 / \"hello\", \"big\",\"small\")"
								"\n\t\t"
							"BB7=ifThenElse(\"big\",\"small\")\n\t\t"
							"E0=isError(BB6)\n\t\t"
							"E1=isError(BB7)\n\t\t"
							"BB8=ifThenElse(BB > 5, 4 / 0,\"small\")\n\t\t"
							"BB9=ifThenElse(BC > 5, \"big\", 4 / 0)\n\t\t"
							"BB10=ifThenElse(0.0, \"then\", \"else\")\n\t\t"
							"BB11=ifThenElse(1.0, \"then\", \"else\")\n\t\t"
							"BB12=ifThenElse(3.7, \"then\", \"else\")\n\t\t"
							"BB13=ifThenElse(\"\", \"then\", \"else\")\n\t\t"
							"E2=isError(BB13)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_string_list() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using stringlist*() functions.");
	emit_comment("The classad string is not printed here due to its large "
		"size.");
    const char* classad_string = 	
		"\tO0=stringlistsize(\"A ,0 ,C\")\n\t\t"
		"O1=stringlistsize(\"\")\n\t\t"
		"O2=stringlistsize(\"A;B;C;D;E\",\";\")\n\t\t"
		"O3=isError(stringlistsize(\"A;B;C;D;E\",true))\n\t\t"
		"O4=isError(stringlistsize(true,\"A;B;C;D;E\"))\n\t\t"
		"O5=stringlistsize(\"A B C,D\")\n\t\t"
		"O6=stringlistsize(\"A B C,,,,,D\")\n\t\t"
		"O7=stringlistsize(\"A B C ; D\",\";\")\n\t\t"
		"O8=stringlistsize(\"A B C;D\",\" ; \")\n\t\t"
		"O9=stringlistsize(\"A  +B;C$D\",\"$;+\")\n\t\t"

		"P0=stringlistsum(\"1,2,3\")\n\t\t"
		"P1=stringlistsum(\"\")\n\t\t"
		"P2=stringlistsum(\"1;2;3\",\";\")\n\t\t"
		"P3=isError(stringlistsum(\"1;2;3\",true))\n\t\t"
		"P4=isError(stringlistsum(true,\"1;2;3\"))\n\t\t"
		"P5=isError(stringlistsum(\"this, list, bad\"))\n\t\t"
		"P6=stringlistsum(\"1,2.0,3\")\n\t\t"

		"Q0=stringlistmin(\"-1,2,-3\")\n\t\t"
		"Q1=isUndefined(stringlistmin(\"\"))\n\t\t"
		"Q2=stringlistmin(\"1;2;3\",\";\")\n\t\t"
		"Q3=isError(stringlistmin(\"1;2;3\",true))\n\t\t"
		"Q4=isError(stringlistmin(true,\"1;2;3\"))\n\t\t"
		"Q5=isError(stringlistmin(\"this, list, bad\"))\n\t\t"
		"Q6=isError(stringlistmin(\"1;A;3\",\";\"))\n\t\t"
		"Q7=stringlistmin(\"1,-2.0,3\")\n\t\t"

		"R0=stringlistmax(\"1 , 4.5, -5\")\n\t\t"
		"R1=isUndefined(stringlistmax(\"\"))\n\t\t"
		"R2=stringlistmax(\"1;2;3\",\";\")\n\t\t"
		"R3=isError(stringlistmax(\"1;2;3\",true))\n\t\t"
		"R4=isError(stringlistmax(true,\"1;2;3\"))\n\t\t"
		"R5=isError(stringlistmax(\"this, list, bad\"))\n\t\t"
		"R6=isError(stringlistmax(\"1;A;3\",\";\"))\n\t\t"
		"R7=stringlistmax(\"1,-2.0,3.0\")\n\t\t"

		"S0=stringlistavg(\"10, 20, 30, 40\")\n\t\t"
		"S1=stringlistavg(\"\")\n\t\t"
		"S2=stringlistavg(\"1;2;3\",\";\")\n\t\t"
		"S3=isError(stringlistavg(\"1;2;3\",true))\n\t\t"
		"S4=isError(stringlistavg(true,\"1;2;3\"))\n\t\t"
		"S5=isError(stringlistavg(\"this, list, bad\"))\n\t\t"
		"S6=isError(stringlistavg(\"1;A;3\",\";\"))\n\t\t"
		"S7=stringlistavg(\"1,-2.0,3.0\")\n\t\t"

		"U0=stringlistmember(\"green\", \"red, blue, green\")\n\t\t"
		"U1=stringlistmember(\"green\",\"\")\n\t\t"
		"U2=stringlistmember(\"green\", \"red; blue; green\",\";\")\n\t\t"
		"U3=isError(stringlistmember(\"green\",\"1;2;3\",true))\n\t\t"
		"U4=isError(stringlistmember(\"green\",true,\";\"))\n\t\t"
		"U5=isError(stringlistmember(true,\"green\",\";\"))\n\t\t"
		"U6=isError(stringlistmember(\"this, list, bad\"))\n\t\t"
		"U7=isError(stringlistmember(\"1;A;3\",\";\"))\n\t\t"
		"U8=stringlistmember(\"-2.9\",\"1,-2.0,3.0\")\n\t\t"
		"U=stringlistmember(\"green\", \"red, blue, green\")\n\t\t"
		"V=stringlistimember(\"ReD\", \"RED, BLUE, GREEN\")";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_string() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using string().");
    const char* classad_string = 	"\tBC0=string(\"-3\")\n\t\t"
							"BC1=string(123)\n\t\t"
							"E0=isError(BC1)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_strcat() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using strcat().");
    const char* classad_string = 	"\tBC0=strcat(\"-3\",\"3\")\n\t\t"
							"BC1=strcat(\"a\",\"b\",\"c\",\"d\",\"e\",\"f\","
								"\"g\")";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_floor() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using floor().");
    const char* classad_string = 	"\tBC0=floor(\"-3\")\n\t\t"
							"BC1=floor(\"-3.4\")\n\t\t"
							"BC2=floor(\"3\")\n\t\t"
							"BC3=floor(5)\n\t\t"
							"BC4=floor(5.2)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_ceiling() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using ceiling().");
    const char* classad_string = 	"\tBC0=ceiling(\"-3\")\n\t\t"
							"BC1=ceiling(\"-3.4\")\n\t\t"
							"BC2=ceiling(\"3\")\n\t\t"
							"BC3=ceiling(5)\n\t\t"
							"BC4=ceiling(5.2)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_round() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using round().");
    const char* classad_string = 	"\tBC0=round(\"-3\")\n\t\t"
							"BC1=round(\"-3.5\")\n\t\t"
							"BC2=round(\"3\")\n\t\t"
							"BC3=round(5.5)\n\t\t"
							"BC4=round(5.2)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_random() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using random().");
    const char* classad_string = 	"\tBC1=random(5)\n\t\t"
							"BC2=random()\n\t\t"
							"BC3=random(3.5)\n\t\t"
							"BC4=random(\"-3.5\")\n\t\t"
							"BC5=random(\"-3.5\")\n\t\t"
							"BC6=random(\"-3.5\")\n\t\t"
							"BC7=random(\"3\")\n\t\t"
							"BC8=random(5.5)\n\t\t"
							"BC9=random(5.2)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_is_string() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using isString().");
    const char* classad_string = 	"\tBC3=isString(\"abc\")\n\t\t"
							"BC0=isString(strcat(\"-3\",\"3\"))";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_is_undefined() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using isUndefined().");
    const char* classad_string = 	"\tBB=2\n\t\t"
							"BC=10\n\t\t"
							"BB0=isUndefined(BD)\n\t\t"
							"BB1=isUndefined(BC)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_is_error() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using isError().");
    const char* classad_string =  "\tBC0=isError(random(\"-3\"))\n\t\t"
							"BC1=isError(int(\"this is not an int\"))\n\t\t"
							"BC2=isError(real(\"this is not a float\"))\n\t\t"
							"BC3=isError(floor(\"this is not a float\"))";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_is_integer() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using isInteger().");
    const char* classad_string =  "\tBC1=isInteger(-3.4 )\n\t\t"
							"BC2=isInteger(-3)\n\t\t"
							"BC3=isInteger(\"-3\")\n\t\t"
							"BC4=isInteger( 3.4 )\n\t\t"
							"BC5=isInteger( int(3.4) )\n\t\t"
							"BC6=isInteger(int(\"-3\"))\n\t\t"
							"BC7=isInteger(3)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_is_real() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using isReal().");
    const char* classad_string =  "\tBC1=isReal(-3.4 )\n\t\t"
							"BC2=isReal(-3)\n\t\t"
							"BC3=isReal(\"-3\")\n\t\t"
							"BC4=isReal( 3.4 )\n\t\t"
							"BC5=isReal( real(3) )\n\t\t"
							"BC6=isReal(real(\"-3\"))\n\t\t"
							"BC7=isReal(3)\n\t\t"
							"BC8=isReal(3,1)\n\t\t"
							"BC9=isError(BC8)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_is_boolean() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using isBoolean().");
    const char* classad_string =  "\tBC1=isBoolean(isReal(-3.4 ))";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_substr() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using substr().");
    const char* classad_string =  "\tI0=substr(\"abcdefg\", 3)\n\t\t"
							"I1=substr(\"abcdefg\", 3, 2)\n\t\t"
							"I2=substr(\"abcdefg\", -2, 1)\n\t\t"
							"I3=substr(\"abcdefg\", 3, -1)\n\t\t"
							"I4=substr(\"abcdefg\", 3, -9)\n\t\t"
							"I5=substr(\"abcdefg\", 3.3, -9)\n\t\t"
							"I6=substr(foo, 3, -9)\n\t\t"
							"E0=isError(I5)\n\t\t"
							"E1=isUndefined(I6)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_formattime() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using formattime().");
    const char* classad_string =  "\tI0=formattime()\n\t\t"
							"I1=formattime(CurrentTime)\n\t\t"
							"I2=formattime(CurrentTime,\"%c\")\n\t\t"
							"I3=formattime(1174737600,\"%m/%d/%y\")\n\t\t"
							"I4=formattime(-231)\n\t\t"
							"I5=formattime(1174694400,1174694400)\n\t\t"
							"E0=isError(I4)\n\t\t"
							"E1=isError(I5)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", "%s", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_strcmp() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string when using strcmp().");
    const char* classad_string =  "\tJ0=strcmp(\"ABCDEFgxx\", \"ABCDEFg\")\n\t\t"
							"J1=strcmp(\"BBBBBBBxx\", \"CCCCCCC\")\n\t\t"
							"J2=strcmp(\"AbAbAbAb\", \"AbAbAbAb\")\n\t\t"
							"J3=strcmp(1+1, \"2\")\n\t\t"
							"J4=strcmp(\"2\", 1+1)\n\t\t"
							"K0=stricmp(\"ABCDEFg\", \"abcdefg\")\n\t\t"
							"K1=stricmp(\"ffgghh\", \"aabbcc\")\n\t\t"
							"K2=stricmp(\"aBabcd\", \"ffgghh\")\n\t\t"
							"K3=stricmp(1+1, \"2\")\n\t\t"
							"K4=stricmp(\"2\", 1+1)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_attrnm() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string with an attribute name.");
    const char* classad_string =  "\tT012=t";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_regexp() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string with regexp().");
	emit_comment("The classad string is not printed here due to its large "
		"size.");
    const char* classad_string =  
		"W0=regexp(\"[Mm]atcH.i\", \"thisisamatchlist\",\"i\")\n\t\t"
		"W1=regexp(20, \"thisisamatchlist\", \"i\")\n\t\t"
		"E1=isError(W1)\n\t\t" 
		"W2=regexp(\"[Mm]atcH.i\", 20, \"i\")\n\t\t"
		"E2=isError(W2)\n\t\t" 
		"W3=regexp(\"[Mm]atcH.i\", \"thisisamatchlist\", 20)\n\t\t"
		"E3=isError(W3)\n\t\t" 
		"W4=regexp(\"[Mm]atcH.i\", \"thisisalist\", \"i\")\n\t\t"
		"W5=regexp(\"[Mm]atcH.i\", \"thisisamatchlist\")\n\t\t"
		"W6=regexp(\"([Mm]+[Nn]+)\", \"aaaaaaaaaabbbmmmmmNNNNNN\", \"i\")\n\t\t"
		"X0=regexps(\"([Mm]at)c(h).i\", \"thisisamatchlist\", \"one is \\1 two "
			"is \\2\")\n\t\t"
		"X1=regexps(\"([Mm]at)c(h).i\", \"thisisamatchlist\", \"one is \\1 two "
			"is \\2\",\"i\")\n\t\t"
		"X2=regexps(20 , \"thisisamatchlist\", \"one is \\1 two is \\2\","
			"\"i\")\n\t\t"
		"E4=isError(X2)\n\t\t" 
		"X3=regexps(\"([Mm]at)c(h).i\", 20 , \"one is \\1 two is \\2\","
			"\"i\")\n\t\t"
		"E5=isError(X3)\n\t\t" 
		"X4=regexps(\"([Mm]at)c(h).i\", \"thisisamatchlist\", 20 ,\"i\")\n\t\t"
		"E6=isError(X4)\n\t\t" 
		"X5=regexps(\"([Mm]at)c(h).i\", \"thisisamatchlist\", \"one is \\1 two "
			"is \\2\",20)\n\t\t"
		"E7=isError(X5)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_stringlists_regexpmember() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string with stringlists_regexpmember().");
	emit_comment("The classad string is not printed here due to its large "
		"size.");
    const char* classad_string =  
		"\tU0=stringlist_regexpMember(\"green\", \"red, blue, green\")\n\t\t"
		"U1=stringlist_regexpMember(\"green\", \"red; blue; green\",\"; \")"
			"\n\t\t"
		"U2=stringlist_regexpMember(\"([e]+)\", \"red, blue, green\")\n\t\t"
		"U3=stringlist_regexpMember(\"([p]+)\", \"red, blue, green\")\n\t\t"
		"W0=stringlist_regexpMember(\"[Mm]atcH.i\", \"thisisamatchlist\", \" ,"
			"\", \"i\")\n\t\t"
		"W1=stringlist_regexpMember(20, \"thisisamatchlist\", \"i\")\n\t\t"
		"E1=isError(W1)\n\t\t" 
		"W2=stringlist_regexpMember(\"[Mm]atcH.i\", 20, \"i\")\n\t\t"
		"E2=isError(W2)\n\t\t" 
		"W3=stringlist_regexpMember(\"[Mm]atcH.i\", \"thisisamatchlist\", "
			"20)\n\t\t"
		"E3=isError(W3)\n\t\t" 
		"W7=stringlist_regexpMember(\"[Mm]atcH.i\", \"thisisamatchlist\", \" "
			",\", 20)\n\t\t"
		"E4=isError(W7)\n\t\t" 
		"W4=stringlist_regexpMember(\"[Mm]atcH.i\", \"thisisalist\", \" ,\", "
			"\"i\")\n\t\t"
		"W5=stringlist_regexpMember(\"[Mm]atcH.i\", \"thisisamatchlist\")\n\t\t"
		"W6=stringlist_regexpMember(\"([Mm]+[Nn]+)\", "
			"\"aaaaaaaaaabbbmmmmmNNNNNN\", \" ,\", \"i\")";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", "");
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_init_from_string_various() {
	emit_test("Test that initAdFromString() correctly initializes the classad "
		" to the given string with various classad functions.");
    const char* classad_string =  "\tD1=Time()\n\t\t"
							"D2=Interval(60)\n\t\t"
							"D3=Interval(3600)\n\t\t"
							"D4=Interval(86400)\n\t\t"
		                    "E=sharedstring()\n\t\t"
                            "G=sharedinteger(2)\n\t\t"
	                        "H=sharedfloat(3.14)\n\t\t"
							"L=toupper(\"AbCdEfg\")\n\t\t"
							"M=toLower(\"ABCdeFg\")\n\t\t"
							"N0=size(\"ABC\")\n\t\t"
							"N1=size(\"\")\n\t\t"
							"N2=size(foo)\n\t\t"
							"E0=isUndefined(N2)";
	ClassAd* classad = new ClassAd;
	initAdFromString(classad_string, *classad);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_param("Error Message", "NULL");
	emit_output_expected_header();
	emit_param("Classad != NULL", "TRUE");
	emit_output_actual_header();
	emit_param("Classad != NULL", tfstr(classad != NULL));
	if(classad == NULL) {
		delete classad;
		FAIL;
	}
	delete classad;
	PASS;
}

static bool test_int_invalid() {
	emit_test("Test that LookupInteger() returns 0 for an attribute with an "
		"invalid integer actual.");
    const char* classad_string = "\tB=int(\"this is not a number\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal == 1) {
		FAIL;
	}
	PASS;
}

static bool test_int_false() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"of an attribute with the actual false.");
    const char* classad_string = "\tB=int(false)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 0;
	int retVal = classad.LookupInteger("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal == 0 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_int_true() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"of an attribute with the actual true.");
    const char* classad_string = "\tB=int(true)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 1;
	int retVal = classad.LookupInteger("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal == 0 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_int_float_negative_quotes() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"of an attribute with a negative float actual enclosed in quotes.");
    const char* classad_string = "\tB=int(\"-3.4\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -3;
	int retVal = classad.LookupInteger("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal == 0 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_int_float_negative() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"of an attribute with a negative float actual.");
    const char* classad_string = "\tB=int(-3.4)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -3;
	int retVal = classad.LookupInteger("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal == 0 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_int_float_positive() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"of an attribute with a positive float actual.");
    const char* classad_string = "\tB=int(3.4)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal == 0 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_int_int_positive() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"of an attribute with a positive integer actual.");
    const char* classad_string = "\tB=int(3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal == 0 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_int_error() {
	emit_test("Test LookupBool() on an attribute in a classad that uses "
		"isError() of another attribute that uses int() incorrectly.");
	const char* classad_string = "\tA=int(\"this is not a number\")\n\t\t"
							"B=isError(A)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	classad.LookupBool("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "B");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_real_invalid() {
	emit_test("Test that LoookupFloat() returns 0 for an attribute that uses "
		"real() with an invalid float actual.");
    const char* classad_string = "\tB=real(\"this is not a number\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0;
	int retVal = classad.LookupFloat("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal == 1) {
		FAIL;
	}
	PASS;
}

static bool test_real_false() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"of an attribute that uses real() with the actual false.");
    const char* classad_string = "\tB=real(false)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 0;
	int retVal = classad.LookupFloat("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal == 0 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_real_true() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"of an attribute that uses real() with the actual true.");
    const char* classad_string = "\tB=real(true)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 1;
	int retVal = classad.LookupFloat("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal == 0 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_real_float_negative_quotes() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"of an attribute that uses real() with a negative float actual enclosed"
		" in quotes.");
    const char* classad_string = "\tB=real(\"-3.4\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0f, expect = -3.4f;
	int retVal = classad.LookupFloat("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal == 0 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_real_float_negative() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"of an attribute that uses real() with a negative float actual.");
    const char* classad_string = "\tB=real(-3.4)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0f, expect = -3.4f;
	int retVal = classad.LookupFloat("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal == 0 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_real_float_positive() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"of an attribute that uses real() with a positive float actual.");
    const char* classad_string = "\tB=real(3.4)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0f, expect = 3.4f;
	int retVal = classad.LookupFloat("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal == 0 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_real_int_positive() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"of an attribute that uses real() with a positive integer actual.");
    const char* classad_string = "\tB=real(3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 3;
	int retVal = classad.LookupFloat("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal == 0 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_real_error() {
	emit_test("Test LookupBool() on an attribute in a classad that uses "
		"isError() of another attribute that uses real() incorrectly.");
	const char* classad_string = "\tA=real(\"this is not a number\")\n\t\t"
							"B=isError(A)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false;
	classad.LookupBool("B", actual);
	bool expect = true;
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "B");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_false() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"of an attribute with an ifThenElse() that evaluates to false.");
    const char* classad_string = "\tA=2\n\t\tB=ifThenElse(A > 5, \"big\", \""
		"small\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "small";
	int retVal = classad.LookupString("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal == 0 || strcmp(actual.c_str(), expect)) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_false_error() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"of an attribute with an ifThenElse() that evaluates to false when the "
		"else part evaluates to an error.");
    const char* classad_string = "\tA=2\n\t\tB=ifThenElse(A > 5, 4 / 0, \"small"
		"\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "small";
	int retVal = classad.LookupString("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal == 0 || strcmp(actual.c_str(), expect)) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_false_constant() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"of an attribute with an ifThenElse() that evaluates to false when "
		"evaluating a constant.");
    const char* classad_string = "\tB=ifThenElse(0.0, \"then\", \"else\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "else";
	int retVal = classad.LookupString("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal == 0 || strcmp(actual.c_str(), expect)) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_true() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"of an attribute with an ifThenElse() that evaluates to true.");
    const char* classad_string = "\tA=10\n\t\tB=ifThenElse(A > 5, \"big\", "
		"\"small\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "big";
	int retVal = classad.LookupString("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal == 0 || strcmp(actual.c_str(), expect)) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_true_error() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"of an attribute with an ifThenElse() that evaluates to true when the "
		"then part evaluates to an error.");
    const char* classad_string = "\tA=10\n\t\tB=ifThenElse(A > 5, \"big\", "
		"4 / 0)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "big";
	int retVal = classad.LookupString("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal == 0 || strcmp(actual.c_str(), expect)) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_true_constant1() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"of an attribute with an ifThenElse() that evaluates to true when "
		"evaluating a constant with the actual 1.0.");
    const char* classad_string = "\tB=ifThenElse(1.0, \"then\", \"else\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "then";
	int retVal = classad.LookupString("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal == 0 || strcmp(actual.c_str(), expect)) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_true_constant2() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"of an attribute with an ifThenElse() that evaluates to true when "
		"evaluating a constant with the actual 3.7.");
    const char* classad_string = "\tB=ifThenElse(3.7, \"then\", \"else\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "then";
	int retVal = classad.LookupString("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute Name", "B");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal == 0 || strcmp(actual.c_str(), expect)) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_invalid1() {
	emit_test("Test LookupBool() on an attribute in a classad that uses "
		"isError() on another attribute that uses an ifThenElse() with an "
		"invalid condition.");
	const char* classad_string = "\tA=ifThenElse(4 / \"hello\", \"big\","
		"\"small\")\n\t\tB=isError(A)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	classad.LookupBool("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "B");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_invalid2() {
	emit_test("Test LookupBool() on an attribute in a classad that uses "
		"isError() on another attribute that uses an ifThenElse() with an "
		"empty condition.");
	const char* classad_string = "\tA=ifThenElse(\"\", \"then\", \"else\")"
		"\n\t\tB=isError(A)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	classad.LookupBool("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "B");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_too_few() {
	emit_test("Test LookupBool() on an attribute in a classad that uses "
		"isError() on another attribute that uses an ifThenElse() with too few "
		"arguments.");
	const char* classad_string = "\tA=ifThenElse(\"big\",\"small\")\n\t\t"
		"B=isError(A)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	classad.LookupBool("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "B");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_if_then_else_undefined() {
	emit_test("Test LookupBool() on an attribute in a classad that uses "
		"isUndefined() on another attribute that uses an undefined attribute "
		"in an ifThenElse().");
	const char* classad_string = "\tA=ifThenElse(C > 5, \"big\",\"small\")"
		"\n\t\tB=isUndefined(A)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	classad.LookupBool("B", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "B");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_param("BOOL", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_param("BOOL", "%s", actual?"true":"false");
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_3() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistsize() on a StringList of size 3.");
	const char* classad_string = "\tA1=stringlistsize(\"A ,0 ,C\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_0() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistsize() on a StringList of size 0.");
	const char* classad_string = "\tA1=stringlistsize(\"\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 0;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_5() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistsize() on a StringList of size 5.");
	const char* classad_string = "\tA1=stringlistsize(\"A;B;C;D;E\",\";\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 5;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_default_delim() {
	emit_test("Test that LookupInteger() returns 1 for an attribute using "
		"stringlistsize() on a StringList using the default delimiter.");
	const char* classad_string = "\tA1=stringlistsize(\"A B C,D\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_repeat_delim() {
	emit_test("Test that LookupInteger() returns 1 for an attribute using "
		"stringlistsize() on a StringList with repeating delimiters.");
	const char* classad_string = "\tA1=stringlistsize(\"A B C ; D\",\";\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_non_default_delim() {
	emit_test("Test that LookupInteger() returns 1 for an attribute using "
		"stringlistsize() on a StringList not using the default delimiter.");
	const char* classad_string = "\tA1=stringlistsize(\"A B C ; D\",\";\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_two_delim() {
	emit_test("Test that LookupInteger() returns 1 for an attribute using "
		"stringlistsize() on a StringList using two different delimiters.");
	const char* classad_string = "\tA1=stringlistsize(\"A B C;D\",\" ; \")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_three_delim() {
	emit_test("Test that LookupInteger() returns 1 for an attribute using "
		"stringlistsize() on a StringList using three different delimiters.");
	const char* classad_string = "\tA1=stringlistsize(\"A  +B;C$D\",\"$;+\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_error_end() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual for"
		" an attribute using isError() of stringlistsize() on a StringList with"
		" an invalid delimiter.");
	const char* classad_string = "\tA1=isError(stringlistsize(\"A;B;C;D;E\","
		"true))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%d", actual?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%d", expect?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_size_error_beginning() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual for"
		" an attribute using isError() of stringlistsize() on a StringList with"
		" an invalid string.");
	const char* classad_string = "\tA1=isError(stringlistsize(true,\"A;B;C;D;E"
		"\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", actual?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", expect?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_sum_default() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistsum() on a StringList using the "
		"default delimiter.");
	const char* classad_string = "\tA1=stringlistsum(\"1,2,3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 6;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_sum_empty() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistsum() on an empty StringList.");
	const char* classad_string = "\tA1=stringlistsum(\"\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 0.0;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_sum_non_default() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistsum() on a StringList not using the "
		"default delimiter.");
	const char* classad_string = "\tA1=stringlistsum(\"1;2;3\",\";\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 6;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_sum_both() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistsum() on a StringList that contains "
		"both ints and floats.");
	const char* classad_string = "\tA1=stringlistsum(\"1,2.0,3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 6.0;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_sum_error_end() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError of stringlistsum() on a StringList that"
		" contains an invalid delimiter.");
	const char* classad_string = "\tA1=isError(stringlistsum(\"1;2;3\",true))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_sum_error_beginning() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError of stringlistsum() on a StringList that"
		" contains an invalid string.");
	const char* classad_string = "\tA1=isError(stringlistsum(true,\"1;2;3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_sum_error_all() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError of stringlistsum() on a StringList that"
		" doesn't contain integers.");
	const char* classad_string = "\tA1=isError(stringlistsum(\"this, list, bad"
		"\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_negative() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistmin() on a StringList that contains "
		"negative integers.");
	const char* classad_string = "\tA1=stringlistmin(\"-1,2,-3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_positive() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistmin() on a StringList that contains "
		"positive integers.");
	const char* classad_string = "\tA1=stringlistmin(\"1;2;3\",\";\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_both() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistmin() on a StringList that contains "
		"both ints and floats.");
	const char* classad_string = "\tA1=stringlistmin(\"1,-2.0,3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = -2.0;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_undefined() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isUndefined() of stringlistmin() on an empty "
		"StringList.");
	const char* classad_string = "\tA1=isUndefined(stringlistmin(\"\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_error_end() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmin() on a StringList "
		"with an invalid delimiter.");
	const char* classad_string = "\tA1=isError(stringlistmin(\"1;2;3\",true))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_error_beginning() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmin() on a StringList "
		"with an invalid string.");
	const char* classad_string = "\tA1=isError(stringlistmin(true,\"1;2;3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_error_all() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmin() on a StringList "
		"that doesn't have any numbers.");
	const char* classad_string = "\tA1=isError(stringlistmin(\"this, list, bad"
		"\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_min_error_middle() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmin() on a StringList "
		"with an invalid string in the middle.");
	const char* classad_string = "\tA1=isError(stringlistmin(\"1;A;3\",\";\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_negatve() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistmax() on a StringList that contains "
		"negative actuals.");
	const char* classad_string = "\tA1=stringlistmax(\"1 , 4.5, -5\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 4.5;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_positive() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stringlistmax() on a StringList that contains "
		"positive integers.");
	const char* classad_string = "\tA1=stringlistmax(\"1;2;3\",\";\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_both() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistmax() on a StringList that contains "
		"both integers and floats.");
	const char* classad_string = "\tA1=stringlistmax(\"1,-2.0,3.0\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 3.0;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_undefined() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isUndefined() of stringlistmax() on an empty "
		"StringList.");
	const char* classad_string = "\tA1=isUndefined(stringlistmax(\"\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_error_end() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmax() on a StringList "
		"with an invalid delimiter.");
	const char* classad_string = "\tA1=isError(stringlistmax(\"1;2;3\",true))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_error_beginning() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmax() on a StringList "
		"with an invalid string.");
	const char* classad_string = "\tA1=isError(stringlistmax(true,\"1;2;3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_error_all() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmax() on a StringList "
		"with all invalid strings.");
	const char* classad_string = "\tA1=isError(stringlistmax(\"this, list, bad"
		"\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_max_error_middle() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistmax() on a StringList "
		"with an invalid string in the middle.");
	const char* classad_string = "\tA1=isError(stringlistmax(\"1;A;3\",\";\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_default() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistavg() on a StringList that uses the "
		"default delimiter.");
	const char* classad_string = "\tA1=stringlistavg(\"10, 20, 30, 40\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 25.0;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_empty() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistavg() on an empty StringList.");
	const char* classad_string = "\tA1=stringlistavg(\"\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 0.0;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_non_default() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistavg() on a StringList that doesn't "
		"use the default delimiter.");
	const char* classad_string = "\tA1=stringlistavg(\"1;2;3\",\";\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 2.0;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_both() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using stringlistavg() on a StringList that contains "
		"both integers and floats.");
	const char* classad_string = "\tA1=stringlistavg(\"1,-2.0,3.0\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0f, expect = 0.666667f;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "%f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || !floats_close(actual, expect)) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_error_end() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistavg() on a StringList "
		"with an invalid delimiter.");
	const char* classad_string = "\tA1=isError(stringlistavg(\"1;2;3\",true))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_error_beginning() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistavg() on a StringList "
		"with an invalid string.");
	const char* classad_string = "\tA1=isError(stringlistavg(true,\"1;2;3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_error_all() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistavg() on a StringList "
		"with all invalid strings.");
	const char* classad_string = "\tA1=isError(stringlistavg(\"this, list, bad"
		"\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_avg_error_middle() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of stringlistavg() on a StringList "
		"with an invalid string in the middle.");
	const char* classad_string = "\tA1=isError(stringlistavg(\"1;A;3\",\";\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_member() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlistmember() on a typical StringList.");
	const char* classad_string = "\tA1=stringlistmember(\"green\", \"red, blue,"
		" green\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_list_member_case() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlistmember() on a typical StringList "
		"with different cases.");
	const char* classad_string = "\tA1=stringlistimember(\"ReD\", \"RED, BLUE, "
		"GREEN\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_string_negative_int() {
	emit_test("Test that LookupString() returns 1 for an attribute using "
		"string() with a negative int.");
	const char* classad_string = "\tA1=string(\"-3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_string_positive_int() {
	emit_test("Test that LookupString() returns 1 for an attribute using "
		"string() with a positive int.");
	const char* classad_string = "\tA1=string(123)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_strcat_short() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for a short attribute using strcat().");
	const char* classad_string = "\tA1=strcat(\"-3\",\"3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "-33";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_strcat_long() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for a long attribute using strcat().");
	const char* classad_string = "\tA1=strcat(\"a\",\"b\",\"c\",\"d\",\"e\","
		"\"f\",\"g\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "abcdefg";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_floor_negative_int() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using floor() on a negative integer.");
	const char* classad_string = "\tA1=floor(\"-3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_floor_negative_float() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using floor() on a negative float.");
	const char* classad_string = "\tA1=floor(\"-3.4\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -4;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_floor_positive_integer() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using floor() on a positive integer.");
	const char* classad_string = "\tA1=floor(\"3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_floor_positive_integer_wo() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using floor() on a positive integer without "
		"quotes around the number.");
	const char* classad_string = "\tA1=floor(5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 5;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_floor_positive_float_wo() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using floor() on a positive float without "
		"quotes around the number.");
	const char* classad_string = "\tA1=floor(5.2)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 5;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_ceiling_negative_int() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using ceiling() on a negative integer.");
	const char* classad_string = "\tA1=ceiling(\"-3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_ceiling_negative_float() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using ceiling() on a negative float.");
	const char* classad_string = "\tA1=ceiling(\"-3.4\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_ceiling_positive_integer() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using ceiling() on a positive integer.");
	const char* classad_string = "\tA1=ceiling(\"3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_ceiling_positive_integer_wo() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using ceiling() on a positive integer without "
		"quotes around the number.");
	const char* classad_string = "\tA1=ceiling(5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 5;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_ceiling_positive_float_wo() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using ceiling() on a positive float without "
		"quotes around the number.");
	const char* classad_string = "\tA1=ceiling(5.2)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 6;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_round_negative_int() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using round() on a negative integer.");
	const char* classad_string = "\tA1=round(\"-3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_round_negative_float() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using round() on a negative float.");
	const char* classad_string = "\tA1=round(\"-3.5\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
// Beginning with Visual Studio 2015, we use the c-runtime rint() function to implement round just like we do on *nix, so it has the same 'flaw'
#if defined WIN32 && _MSC_VER < 1900
	int expect = -3;
#else
	emit_problem("The correct answer should be -3 (round toward 0), but no matter the rounding mode, glibc always returns -4");
	int expect = -4;
#endif
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_round_positive_int() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using round() on a positive integer.");
	const char* classad_string = "\tA1=round(\"3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_round_positive_float_wo_up() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using round() on a positive float without "
		"quotes surrounding the number and the function rounds up.");
	const char* classad_string = "\tA1=round(5.5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 6;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_round_positive_float_wo_down() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using round() on a positive float without "
		"quotes surrounding the number and the function rounds down.");
	const char* classad_string = "\tA1=round(5.2)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 5;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_random_integer() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using random() on a positive integer.");
	const char* classad_string = "\tA1=random(5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual =-1, expect = 5;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "< %d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual >= expect) {
		FAIL;
	}
	PASS;
}

static bool test_random() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using random() without an argument.");
	const char* classad_string = "\tA1=random()";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 1;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "< %f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || actual >= expect) {
		FAIL;
	}
	PASS;
}

static bool test_random_float() {
	emit_test("Test that LookupFloat() returns 1 and sets the correct actual "
		"for an attribute using random() on a positive float.");
	const char* classad_string = "\tA1=random(3.5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	float actual = -1.0, expect = 3.5;
	int retVal = classad.LookupFloat("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("FLOAT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("FLOAT Value", "< %f", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("FLOAT Value", "%f", actual);
	if(retVal != 1 || actual >= expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_string_simple() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isString() on a simple string.");
	const char* classad_string = "\tA1=isString(\"abc\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_is_string_concat() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isString() on a concatenated string.");
	const char* classad_string = "\tA1=isString(strcat(\"-3\",\"3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_is_undefined_true() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isUndefined() on an undefined attribute.");
	const char* classad_string = "\tA1=isUndefined(BD)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;	}
	PASS;
}

static bool test_is_undefined_false() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isUndefined() on an attribute that is "
		"defined.");
	const char* classad_string = "\tBC=10\n\t\tA1=isUndefined(BC)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_error_random() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() on an attribute with an incorrect "
		"usage of random().");
	const char* classad_string = "\tA1=isError(random(\"-3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_error_int() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() on an attribute with an incorrect "
		"usage of int().");
	const char* classad_string = "\tA1=isError(int(\"this is not an int\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_error_real() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() on an attribute with an incorrect "
		"usage of real().");
	const char* classad_string = "\tA1=isError(real(\"this is not a float\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_error_floor() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() on an attribute with an incorrect "
		"usage of floor().");
	const char* classad_string = "\tA1=isError(floor(\"this is not a float\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_integer_false_negative() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isInteger() on a negative real.");
	const char* classad_string = "\tA1=isInteger(-3.4 )";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_integer_false_positive() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isInteger() on a positive real.");
	const char* classad_string = "\tA1=isInteger(3.4 )";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_integer_false_quotes() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isInteger() on an integer in quotes.");
	const char* classad_string = "\tA1=isInteger(\"-3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_integer_true_negative() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isInteger() on a negative integer.");
	const char* classad_string = "\tA1=isInteger(-3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_integer_true_int() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isInteger() along with int() on a positive "
		"real.");
	const char* classad_string = "\tA1=isInteger( int(3.4) )";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_integer_true_int_quotes() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isInteger() along with int() on a negative "
		"integer in quotes.");
	const char* classad_string = "\tA1=isInteger(int(\"-3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_integer_true_positive() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isInteger() on a positive integer.");
	const char* classad_string = "\tA1=isInteger(3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_false_negative_int() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isReal() on a negative integer.");
	const char* classad_string = "\tA1=isReal(-3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_false_negative_int_quotes() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isReal() on a negative integer in "
		"quotes.");
	const char* classad_string = "\tA1=isReal(\"-3\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_false_positive_int() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isReal() on a positive integer.");
	const char* classad_string = "\tA1=isReal(3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_true_negative() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isReal() on a negative real.");
	const char* classad_string = "\tA1=isReal(-3.4 )";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_true_positive() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isReal() on a positive real.");
	const char* classad_string = "\tA1=isReal( 3.4 )";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_true_real() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isReal() along with real() on a positive "
		"integer.");
	const char* classad_string = "\tA1=isReal( real(3) )";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_true_real_quotes() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isReal() along with real() on a negative "
		"integer in quotes.");
	const char* classad_string = "\tA1=isReal(real(\"-3\"))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_real_error() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of an attriute that uses isReal() "
		"incorrectly.");
	const char* classad_string = "\tBC8=isReal(3,1)\n\t\tA1=isError(BC8)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_boolean() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isBoolean() along with isReal() on a negative "
		"real.");
	const char* classad_string = "\tA1=isBoolean(isReal(-3.4 ))";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval(tfstr(expect));
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_substr_end() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using substr() on the end of a string.");
	const char* classad_string = "\tA1=substr(\"abcdefg\", 3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "defg";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_substr_middle() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using substr() on the middle of a string.");
	const char* classad_string = "\tA1=substr(\"abcdefg\", 3, 2)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "de";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_substr_negative_index() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using substr() with a negative starting index.");
	const char* classad_string = "\tA1=substr(\"abcdefg\", -2, 1)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "f";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_substr_negative_length() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using substr() with a negative length.");
	const char* classad_string = "\tA1=substr(\"abcdefg\", 3, -1)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "def";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_substr_out_of_bounds() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using substr() with an out of bounds length.");
	const char* classad_string = "\tA1=substr(\"abcdefg\", 3, -9)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;	}
	PASS;
}

static bool test_substr_error_index() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"substr() with an invalid index.");
	const char* classad_string = "\tI5=substr(\"abcdefg\", 3.3, -9)\n\t\t"
							"A1=isError(I5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_substr_error_string() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"substr() on an invalid string.");
	const char* classad_string = "\tI6=substr(foo, 3, -9)\n\t\t"
							"A1=isUndefined(I6)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_formattime_empty() {
	emit_test("Test that LookupString() returns 1 for an attribute using "
		"formattime() with no arguments.");
	const char* classad_string = "\tA1=formattime()";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_formattime_current() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using formattime() with CurrentTime.");
	emit_comment("Since there is a chance that the each LookupString() may be "
		"run at a different second, we retry the evaluations up to 10 times.");
	const char* classad_string = "\tA0=formattime()\n\t\tA1=formattime("
		"CurrentTime)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	std::string expect;
	int retVal, attempts = 0;
	do {	
		classad.LookupString("A0", expect);
		retVal = classad.LookupString("A1", actual);
		attempts++;
	}while(attempts < 10 && strcmp(actual.c_str(), expect.c_str()) != MATCH);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect.c_str());
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_formattime_current_options() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using formattime() with CurrentTime and the \"%c\" "
		"option.");
	emit_comment("Since there is a chance that the each LookupString() may be "
		"run at a different second, we retry the evaluations up to 10 times.");
	const char* classad_string = "\tA0=formattime()\n\t\tA1=formattime("
		"CurrentTime,\"%c\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	std::string expect;
	int retVal = -1, attempts = 0;
	do {	
		classad.LookupString("A0", expect);
		retVal = classad.LookupString("A1", actual);
		attempts++;
	}while(attempts < 10 && strcmp(actual.c_str(), expect.c_str()) != MATCH);
	emit_input_header();
	emit_param("ClassAd", "%s", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect.c_str());
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_formattime_int() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using formattime() from a integer.");
	const char* classad_string = "\tA1=formattime(1174737600,\"%m/%d/%y\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "03/24/07";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_formattime_error() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"formattime() incorrectly.");
	const char* classad_string = "\tI5=formattime(1174694400,1174694400)\n\t\t"
		"A1=isError(I5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_strcmp_positive() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using strcmp() of two strings that should result in "
		"a positive number.");
	const char* classad_string = "\tA1=strcmp(\"ABCDEFgxx\", \"ABCDEFg\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "> 0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual <= 0) {
		FAIL;
	}
	PASS;
}

static bool test_strcmp_negative() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using strcmp() of two strings that should result in "
		"a negative number.");
	const char* classad_string = "\tA1=strcmp(\"BBBBBBBxx\", \"CCCCCCC\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "< 0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual >= 0) {
		FAIL;
	}
	PASS;
}

static bool test_strcmp_equal() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using strcmp() of two strings that should give a "
		"result of 0.");
	const char* classad_string = "\tA1=strcmp(\"AbAbAbAb\", \"AbAbAbAb\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != 0) {
		FAIL;
	}
	PASS;
}

static bool test_strcmp_convert1() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using strcmp() of two strings that should give a "
		"result of 0 after converting the first argument to a string.");
	const char* classad_string = "\tA1=strcmp(1+1, \"2\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != 0) {
		FAIL;
	}
	PASS;
}

static bool test_strcmp_convert2() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using strcmp() of two strings that should give a "
		"result of 0 after converting the second argument to a string.");
	const char* classad_string = "\tA1=strcmp(\"2\", 1+1)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != 0) {
		FAIL;
	}
	PASS;
}

static bool test_stricmp_equal() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stricmp() of two strings that should give a "
		"result of 0.");
	const char* classad_string = "\tA1=stricmp(\"ABCDEFg\", \"abcdefg\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != 0) {
		FAIL;
	}
	PASS;
}

static bool test_stricmp_positive() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stricmp() of two strings that should give a "
		"result of a positive number.");
	const char* classad_string = "\tA1=stricmp(\"ffgghh\", \"aabbcc\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "> 0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual <= 0) {
		FAIL;
	}
	PASS;
}

static bool test_stricmp_negative() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stricmp() of two strings that should give a "
		"result of a negative number.");
	const char* classad_string = "\tA1=stricmp(\"aBabcd\", \"ffgghh\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "< 0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual >= 0) {
		FAIL;
	}
	PASS;
}

static bool test_stricmp_convert1() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stricmp() of two strings that should give a "
		"result of 0 after converting the first argument to a string.");
	const char* classad_string = "\tA1=stricmp(1+1, \"2\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != 0) {
		FAIL;
	}
	PASS;
}

static bool test_stricmp_convert2() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using stricmp() of two strings that should give a "
		"result of 0 after converting the second argument to a string.");
	const char* classad_string = "\tA1=stricmp(\"2\", 1+1)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != 0) {
		FAIL;
	}
	PASS;
}

static bool test_regexp_match_wildcard() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using regexp() that evaluates to a match when using "
		"a wildcard.");
	const char* classad_string = "\tA1=regexp(\"[Mm]atcH.i\", \""
		"thisisamatchlist\", \"i\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexp_match_repeat() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using regexp() that evaluates to a match when using "
		"repeating characters.");
	const char* classad_string = "\tA1=regexp(\"([Mm]+[Nn]+)\", "
		"\"aaaaaaaaaabbbmmmmmNNNNNN\", \"i\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexp_no_match() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using regexp() that doesn't evaluate to a match.");
	const char* classad_string = "\tA1=regexp(\"[Mm]atcH.i\", \"thisisalist\","
		" \"i\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexp_no_match_case() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using regexp() that doesn't evaluate to a match due "
		"to its case.");
	const char* classad_string = "\tA1=regexp(\"[Mm]atcH.i\", \""
		"thisisamatchlist\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexp_error_pattern() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"regexp() with an invalid regexp pattern.");
	const char* classad_string = "\tW1=regexp(20, \"thisisamatchlist\", \"i\")"
		"\n\t\tA1=isError(W1)"; 
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexp_error_target() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"regexp() with an invalid regexp target string.");
	const char* classad_string = "\tW1=regexp(\"[Mm]atcH.i\", 20, \"i\")\n\t\t"
		"A1=isError(W1)"; 
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexp_error_option() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"regexp() with an invalid regexp option.");
	const char* classad_string = "W3=regexp(\"[Mm]atcH.i\", \"thisisamatchlist"
		"\", 20)\n\t\tA1=isError(W3)"; 
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_match() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using regexps() that evaluates to a match.");
	const char* classad_string = "\tA1=regexps(\"([Mm]at)c(h).i\", "
		"\"thisisamatchlist\", \"one is \\1 two is \\2\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "one is mat two is h";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_match_case() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using regexps() that evaluates to a match.");
	const char* classad_string = "\tA1=regexps(\"([Mm]at)c(h).i\", "
		"\"thisisamatchlist\", \"one is \\1 two is \\2\",\"i\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "one is mat two is h";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_error_pattern() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"regexps() with an invalid regexps pattern.");
	const char* classad_string = "\tX2=regexps(20 , \"thisisamatchlist\", \"one is "
		"\\1 two is \\2\",\"i\")\n\t\t"
		"A1=isError(X2)"; 
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_error_target() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"regexps() with an invalid regexps target string.");
	const char* classad_string = "\tX3=regexps(\"([Mm]at)c(h).i\", 20 , \""
		"one is \\1 two is \\2\",\"i\")\n\t\tA1=isError(X3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_error_option() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"regexps() with an invalid regexps option.");
	const char* classad_string = "\tX5=regexps(\"([Mm]at)c(h).i\", \""
		"thisisamatchlist\", \"one is \\1 two is \\2\",20)\n\t\tA1=isError(X5)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_error_return() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"regexps() with an invalid regexps return arg.");
	const char* classad_string = "\tX4=regexps(\"([Mm]at)c(h).i\", \""
		"thisisamatchlist\", 20 ,\"i\")\n\t\tA1=isError(X4)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_full() {
	emit_test("Test that regexps() with 'f' option returns full target "
		"string with a single replacement.");
	const char* classad_string = "\tA1=regexps(\"s(.)\", "
		"\"thisisamatchlist\", \"S\\1\", \"f\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "thiSisamatchlist";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_regexps_full_global() {
	emit_test("Test that regexps() with 'fg' options returns full target "
		"string with global replacement.");
	const char* classad_string = "\tA1=regexps(\"s(.)\", "
		"\"thisisamatchlist\", \"S\\1\", \"fg\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "thiSiSamatchliSt";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_replace() {
	emit_test("Test that replace() returns full target "
		"string with a single replacement.");
	const char* classad_string = "\tA1=replace(\"s(.)\", "
		"\"thisisamatchlist\", \"S\\1\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "thiSisamatchlist";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_replaceall() {
	emit_test("Test that replaceall() returns full target "
		"string with global replacement.");
	const char* classad_string = "\tA1=replaceall(\"s(.)\", "
		"\"thisisamatchlist\", \"S\\1\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "thiSiSamatchliSt";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "%s", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_match_default() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlist_regexpMember() that evaluates to a "
		"match when using the default delimiter.");
	const char* classad_string = "\tA1=stringlist_regexpMember(\"green\", \""
		"red, blue, green\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_match_non_default() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlist_regexpMember() that evaluates to a "
		"match when not using the default delimiter.");
	const char* classad_string = "\tA1=stringlist_regexpMember(\"green\", \""
		"red; blue; green\",\"; \")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_match_case() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlist_regexpMember() that evaluates to a "
		"match when using case insensitive.");
	const char* classad_string = "\tA1=stringlist_regexpMember(\"[Mm]atcH.i\", "
		"\"thisisamatchlist\", \" ,\", \"i\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_match_repeat() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlist_regexpMember() that evaluates to a "
		"match when using repeating characters.");
	const char* classad_string = "\tA1=stringlist_regexpMember(\"([Mm]+[Nn]+)"
		"\", \"aaaaaaaaaabbbmmmmmNNNNNN\", \" ,\", \"i\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_no_match_multiple() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlist_regexpMember() that doesn't evaluate"
		" to a match due to multiple misses.");
	const char* classad_string = "\tA1=stringlist_regexpMember(\"([p]+)\", \"red, "
		"blue, green\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_no_match() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlist_regexpMember() that doesn't evaluate"
		" to a match.");
	const char* classad_string = "\tA1=stringlist_regexpMember(\"[Mm]atcH.i\", "
		"\"thisisalist\", \" ,\", \"i\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_no_match_case() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using stringlist_regexpMember() that doesn't evaluate"
		" to a match due to case.");
	const char* classad_string = "\tA1=stringlist_regexpMember(\"[Mm]atcH.i\", "
		"\"thisisamatchlist\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_error_pattern() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"stringlist_regexpMember() with an invalid regexp pattern.");
	const char* classad_string = 
		"\tW1=stringlist_regexpMember(20, \"thisisamatchlist\", \"i\")\n\t\t"
		"A1=isError(W1)"; 
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_error_target() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"stringlist_regexpMember() with an invalid regexp target.");
	const char* classad_string = "\tW2=stringlist_regexpMember(\"[Mm]atcH.i\", "
		"20, \"i\")\n\t\tA1=isError(W2)"; 
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_error_delim() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"stringlist_regexpMember() with an invalid delimiter.");
	const char* classad_string = "\tW3=stringlist_regexpMember(\"[Mm]atcH.i\", "
		"\"thisisamatchlist\", 20)\n\t\tA1=isError(W3)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_stringlist_regexp_member_error_option() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using isError() of another attribute that uses "
		"stringlist_regexpMember() with an invalid regexp option.");
	const char* classad_string = "\tW7=stringlist_regexpMember(\"[Mm]atcH.i\", "
		"\"thisisamatchlist\", \" ,\", 20)\n\t\tA1=isError(W7)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_random_different() {
	emit_test("Test that LookupInteger() sets the correct actual for an "
		"attribute using random(256), in particular check that it generates "
		"different numbers.");
	emit_comment("This test will fail if random() generates the same number "
		"10 times in a row, although this is highly unlikely.");
	const char* classad_string = "\tA1 = random(256)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = -2, i;
	bool different_numbers = false;
	classad.LookupInteger("A1", expect);
	for(i = 0; i < 10; i++) {
		classad.LookupInteger("A1", actual);
		different_numbers |= (actual != expect);
	}
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_param("Different Numbers Generated", "TRUE");
	emit_output_actual_header();
	emit_param("Different Numbers Generated", tfstr(different_numbers));
	if(!different_numbers) {
		FAIL;
	}
	PASS;
}

static bool test_random_range() {
	emit_test("Test that LookupInteger() sets the correct actual for an "
		"attribute using random(), in particular check that it generates "
		"random numbers within the correct range.");
	const char* classad_string = "\tA1 = random(10)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, i;
	bool in_range = true;
	for(i = 0; i < 1000 && in_range; i++) {
		classad.LookupInteger("A1", actual);
		in_range = (actual >= 0 && actual < 10);
	}
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_param("Numbers Generated in Range", "TRUE");
	emit_output_actual_header();
	emit_param("Numbers Generated in Range", tfstr(in_range));
	if(!in_range) {
		FAIL;
	}
	PASS;
}

static bool test_equality() {
	emit_test("Test equality after parsing a classad string into an ExprTree "
		"and MyString.");
	const char* classad_string = "\tFoo = 3";
	ExprTree *e1, *e2;
	std::string n1, n2;
	ParseLongFormAttrValue(classad_string, n1, e1);
	ParseLongFormAttrValue(classad_string, n2, e2);
	emit_input_header();
	emit_param("STRING", classad_string);
	emit_output_expected_header();
	emit_param("ExprTree Equality", "TRUE");
	emit_param("MyString Equality", "TRUE");
	emit_output_actual_header();
	emit_param("ExprTree Equality", tfstr((*e1) == (*e2)));
	emit_param("MyString Equality", tfstr(n1 == n2));
	emit_param("n1", n1.c_str());
	emit_param("n2", n2.c_str());
	if(!((*e1) == (*e2)) || !(n1 == n2)) {
		delete(e1); delete(e2);
		FAIL;
	}
	delete(e1); delete(e2);
	PASS;
}

static bool test_inequality() {
	emit_test("Test inequality after parsing a classad string into an "
		"ExprTree and MyString.");
	const char* classad_string1  = "Foo = 3";
	const char* classad_string2  = "Bar = 5";
	ExprTree *e1, *e2;
	std::string n1, n2;
	ParseLongFormAttrValue(classad_string1, n1, e1);
	ParseLongFormAttrValue(classad_string2, n2, e2);
	emit_input_header();
	emit_param("STRING 1", classad_string1);
	emit_param("STRING 2", classad_string2);
	emit_output_expected_header();
	emit_param("ExprTree Inequality", "TRUE");
	emit_param("MyString Inequality", "TRUE");
	emit_output_actual_header();
	emit_param("ExprTree Inequality", tfstr(!((*e1) == (*e2))));
	emit_param("MyString Inequality", tfstr(!(n1 == n2)));
	if(((*e1) == (*e2)) || (n1 == n2)) {
		delete e1; delete e2;
		FAIL;
	}
	delete e1; delete e2;
	PASS;
}

static bool test_operators_short_or() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using short-circuiting with logical OR.");
	const char* classad_string = "\tA1 = TRUE || ERROR";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_operators_no_short_or() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute not using short-circuiting with logical OR.");
	const char* classad_string = "\tA1 = isError(FALSE || ERROR)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_operators_short_and() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using short-circuiting with logical AND.");
	const char* classad_string = "\tA1 = FALSE && ERROR";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = true, expect = false;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_operators_short_and_error() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute using short-circuiting with logical AND that "
		"evaluates to an ERROR.");
	const char* classad_string = "\tA1 = isError(\"foo\" && ERROR)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_operators_no_short_and() {
	emit_test("Test that LookupBool() returns 1 and sets the correct actual "
		"for an attribute not using short-circuiting with logical AND.");
	const char* classad_string = "\tA1 = isError(TRUE && ERROR)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_scoping_my() {
	emit_test("Test that EvalInteger() returns 1 and sets the correct actual "
		"for an attribute in the classad's slope.");
	const char* classad_string1 = "\tA = MY.X\n\t\tB = TARGET.X\n\t\tC = MY.Y"
		"\n\t\tD = TARGET.Y\n\t\tE = Y\n\t\tG = MY.Z\n\t\tH = TARGET.Z\n\t\t"
		"J = TARGET.K\n\t\tL = 5\n\t\tX = 1\n\t\tZ = 4";
	const char* classad_string2 = "X = 2\n\t\tY = 3\n\t\tK = TARGET.L";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	int actual = -1, expect = 1;
	int retVal = EvalInteger("A", &classad1, &classad2, actual);
	emit_input_header();
	emit_param("ClassAd", classad_string1);
	emit_param("Attribute", "A");
	emit_param("Target", classad_string2);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_scoping_my_dup() {
	emit_test("Test that EvalInteger() returns 1 and sets the correct actual "
		"for an attribute in the classad's slope in which there is also an "
		"attribute with the same name in the target classad's scope.");
	const char* classad_string1 = "\tA = MY.X\n\t\tB = TARGET.X\n\t\tC = MY.Y"
		"\n\t\tD = TARGET.Y\n\t\tE = Y\n\t\tG = MY.Z\n\t\tH = TARGET.Z\n\t\t"
		"J = TARGET.K\n\t\tL = 5\n\t\tX = 1\n\t\tZ = 4";
	const char* classad_string2 = "X = 2\n\t\tY = 3\n\t\tK = TARGET.L";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	int actual = -1, expect = 4;
	int retVal = EvalInteger("G", &classad1, &classad2, actual);
	emit_input_header();
	emit_param("ClassAd", classad_string1);
	emit_param("Attribute", "G");
	emit_param("Target", classad_string2);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_scoping_target() {
	emit_test("Test that EvalInteger() returns 1 and sets the correct actual "
		"for an attribute in the target classad's slope.");
	const char* classad_string1 = "\tA = MY.X\n\t\tB = TARGET.X\n\t\tC = MY.Y"
		"\n\t\tD = TARGET.Y\n\t\tE = Y\n\t\tG = MY.Z\n\t\tH = TARGET.Z\n\t\t"
		"J = TARGET.K\n\t\tL = 5\n\t\tX = 1\n\t\tZ = 4";
	const char* classad_string2 = "X = 2\n\t\tY = 3\n\t\tK = TARGET.L";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	int actual = -1, expect = 3;
	int retVal = EvalInteger("D", &classad1, &classad2, actual);
	emit_input_header();
	emit_param("ClassAd", classad_string1);
	emit_param("Attribute", "D");
	emit_param("Target", classad_string2);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_scoping_target_dup() {
	emit_test("Test that EvalInteger() returns 1 and sets the correct actual "
		"for an attribute in the target classad's slope in which there is also "
		"an attribute with the same name in the classad's scope.");
	const char* classad_string1 = "\tA = MY.X\n\t\tB = TARGET.X\n\t\tC = MY.Y"
		"\n\t\tD = TARGET.Y\n\t\tE = Y\n\t\tG = MY.Z\n\t\tH = TARGET.Z\n\t\t"
		"J = TARGET.K\n\t\tL = 5\n\t\tX = 1\n\t\tZ = 4";
	const char* classad_string2 = "X = 2\n\t\tY = 3\n\t\tK = TARGET.L";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	int actual = -1, expect = 2;
	int retVal = EvalInteger("B", &classad1, &classad2, actual);
	emit_input_header();
	emit_param("ClassAd", classad_string1);
	emit_param("Attribute", "B");
	emit_param("Target", classad_string2);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_scoping_both() {
	emit_test("Test that EvalInteger() returns 1 and sets the correct actual "
		"for an attribute in the target classad's slope that is then in the "
		"classad's scope.");
	const char* classad_string1 = "\tA = MY.X\n\t\tB = TARGET.X\n\t\tC = MY.Y"
		"\n\t\tD = TARGET.Y\n\t\tE = Y\n\t\tG = MY.Z\n\t\tH = TARGET.Z\n\t\t"
		"J = TARGET.K\n\t\tL = 5\n\t\tX = 1\n\t\tZ = 4";
	const char* classad_string2 = "X = 2\n\t\tY = 3\n\t\tK = TARGET.L";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	int actual = -1, expect = 5;
	int retVal = EvalInteger("J", &classad1, &classad2, actual);
	emit_input_header();
	emit_param("ClassAd", classad_string1);
	emit_param("Attribute", "J");
	emit_param("Target", classad_string2);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_scoping_my_miss() {
	emit_test("Test that EvalInteger() returns 0 for an attribute not in the "
		"classad's slope.");
	const char* classad_string1 = "\tA = MY.X\n\t\tB = TARGET.X\n\t\tC = MY.Y"
		"\n\t\tD = TARGET.Y\n\t\tE = Y\n\t\tG = MY.Z\n\t\tH = TARGET.Z\n\t\t"
		"J = TARGET.K\n\t\tL = 5\n\t\tX = 1\n\t\tZ = 4";
	const char* classad_string2 = "X = 2\n\t\tY = 3\n\t\tK = TARGET.L";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	int actual = -1;
	int retVal = EvalInteger("C", &classad1, &classad2, actual);
	emit_input_header();
	emit_param("ClassAd", classad_string1);
	emit_param("Attribute", "C");
	emit_param("Target", classad_string2);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal == 1) {
		FAIL;
	}
	PASS;
}

static bool test_scoping_target_miss() {
	emit_test("Test that EvalInteger() returns 0 for an attribute not in the "
		"Target's slope.");
	const char* classad_string1 = "\tA = MY.X\n\t\tB = TARGET.X\n\t\tC = MY.Y"
		"\n\t\tD = TARGET.Y\n\t\tE = Y\n\t\tG = MY.Z\n\t\tH = TARGET.Z\n\t\t"
		"J = TARGET.K\n\t\tL = 5\n\t\tX = 1\n\t\tZ = 4";
	const char* classad_string2 = "X = 2\n\t\tY = 3\n\t\tK = TARGET.L";
	ClassAd classad1, classad2;
	initAdFromString(classad_string1, classad1);
	initAdFromString(classad_string2, classad2);
	int actual = -1;
	int retVal = EvalInteger("H", &classad1, &classad2, actual);
	emit_input_header();
	emit_param("ClassAd", classad_string1);
	emit_param("Attribute", "H");
	emit_param("Target", classad_string2);
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal == 1) {
		FAIL;
	}
	PASS;
}

static bool test_time() {
	emit_test("Test that LookupInteger() returns 1 for an attribute using "
		"time().");
	const char* classad_string = "\tA1=Time()";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != 1) {
		FAIL;
	}
	PASS;
}

static bool test_interval_minute() {
	emit_test("Test that LookupString() returns 1 and sets the correct value "
		"for actual for an attribute using Interval() of one minute.");
	emit_comment("Interval() currently pads the days field with spaces, this "
		"will likely be changed to match the documentation and this test will "
		"then fail. See ticket #1440");
	const char* classad_string = "\tA1=Interval(60)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "1:00";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "'%s'", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "'%s'", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_interval_hour() {
	emit_test("Test that LookupString() returns 1 and sets the correct value "
		"for actual for an attribute using Interval() of one hour.");
	emit_comment("Interval() currently pads the days field with spaces, this "
		"will likely be changed to match the documentation and this test will "
		"then fail. See ticket #1440");
	const char* classad_string = "\tA1=Interval(3600)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "1:00:00";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "'%s'", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "'%s'", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_interval_day() {
	emit_test("Test that LookupString() returns 1 and sets the correct value "
		"for actual for an attribute using Interval() of one hour.");
	emit_comment("Interval() currently pads the days field with spaces, this "
		"will likely be changed to match the documentation and this test will "
		"then fail. See ticket #1440");
	const char* classad_string = "\tA1=Interval(86400)";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "1+00:00:00";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", "'%s'", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "'%s'", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_to_upper() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using toupper().");
	const char* classad_string = "\tA1=toupper(\"AbCdEfg\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "ABCDEFG";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_to_lower() {
	emit_test("Test that LookupString() returns 1 and sets the correct actual "
		"for an attribute using toLower().");
	const char* classad_string = "\tA1=toLower(\"ABCdeFg\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	std::string actual;
	const char* expect = "abcdefg";
	int retVal = classad.LookupString("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("STRING Value", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("STRING Value", "%s", actual.c_str());
	if(retVal != 1 || strcmp(actual.c_str(), expect) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_size_positive() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using size() with a positive result.");
    const char* classad_string = "\tA1=size(\"ABC\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 3;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_size_zero() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using size() with a result of zero.");
    const char* classad_string = "\tA1=size(\"\")";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	int actual = -1, expect = 0;
	int retVal = classad.LookupInteger("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("INT", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("INT Value", "%d", expect);
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("INT Value", "%d", actual);
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_size_undefined() {
	emit_test("Test that LookupInteger() returns 1 and sets the correct actual "
		"for an attribute using isUndefined() of another attribute that uses "
		"size() with an invalid string.");
    const char* classad_string =  "\tN2=size(foo)\n\t\tA1=isUndefined(N2)";
	emit_param("size", "%d", strlen(classad_string));
	ClassAd classad;
	initAdFromString(classad_string, classad);
	bool actual = false, expect = true;
	int retVal = classad.LookupBool("A1", actual);
	emit_input_header();
	emit_param("ClassAd", classad_string);
	emit_param("Attribute", "A1");
	emit_param("Target", "NULL");
	emit_param("BOOL", "");
	emit_output_expected_header();
	emit_retval("1");
	emit_param("BOOL Value", "%s", expect?"true":"false");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	emit_param("BOOL Value", "%s", actual?"true":"false");
	if(retVal != 1 || actual != expect) {
		FAIL;
	}
	PASS;
}


static bool test_nested_ads()
{
	classad::ClassAdParser parser;
	classad::ClassAdUnParser unparser;
	classad::ClassAd ad, ad2;
	classad::ExprTree *tree;

	emit_test("Testing with nested ads");
	
	ad.InsertAttr( "A", 4 );
	if ( !parser.ParseExpression( "{ [ Y = 1; Z = A; ] }", tree ) ) {
		FAIL;
	}
	ad.Insert( "B", tree );
	if ( !parser.ParseExpression( "B[0].Z", tree ) ) {
		FAIL;
	}
	ad.Insert( "C", tree );

	std::string str;
	unparser.Unparse( str, &ad );
	emit_input_header();
	emit_param("ClassAd", str.c_str());
	emit_output_expected_header();
	emit_param("A =", "4");
	emit_param("C =", "4");
	
	int result;
	if ( !ad.EvaluateAttrInt( "A", result ) || result != 4 ) {
		FAIL;
	} 

	if ( !ad.EvaluateAttrInt( "C", result ) || result != 4) {
		FAIL;
	}
	
	PASS;
}

