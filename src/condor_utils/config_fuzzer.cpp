#include "condor_common.h"
#include "condor_config.h"
#include "param_info.h"

#include <cstdint>
#include <string>

/* This is the driver for the config language fuzz tester.
 * Note this is intentionally not in the cmake file, it should not be built
 * by default.
 *
 * To use, compile condor_utils (and classads) with clang
 * -fsanitize=fuzzer-no-link,address then compile this one file with
 * clang -fsanitize=fuzzer,address and link against libcondor_utils.
 * This adds a magic main function here.
 * Then, just run ./a.out -dict=config_fuzzer.dict and let it run for a long time.
 * The dictionary of config keywords and macro functions lets the fuzzer
 * reach things like $F("") that random mutation rarely produces.
 *
 * To fuzz in parallel use -jobs=N -workers=M, -fork crashes in clang 22.
 *
 * The first byte of input selects the parser options, the subsys / local
 * name used for lookups and the expand options, the rest is parsed exactly as
 * a config file would be, then every resulting value is macro expanded the way
 * param() would, by both overloads of expand_macro.
 *
 * include statements are disabled, because "include : cmd |" would run
 * arbitrary commands from the fuzzer input.
 */

static void
IgnoreConfigException(const char * /*msg*/, int /*line*/, const char * /*file*/)
{
}

static MACRO_DEFAULTS FuzzDefaults;

extern "C" int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/) {
	// Without a reporter, config errors like a bad $CHOICE index would spam stderr.
	_EXCEPT_Reporter = IgnoreConfigException;
	FuzzDefaults.size = param_info_init((const void**)&FuzzDefaults.table);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *input, size_t len) {
	if (len < 1) {
		return 0;
	}

	const uint8_t sel = input[0];
	std::string s(input + 1, input + len);

	int opts = CONFIG_OPT_WANT_META | CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO | CONFIG_OPT_NO_INCLUDE_FILE;
	if (sel & 0x01) opts |= CONFIG_OPT_OLD_COM_IN_CONT;
	if (sel & 0x02) opts |= CONFIG_OPT_SMART_COM_IN_CONT;
	if (sel & 0x04) opts |= CONFIG_OPT_COLON_IS_META_ONLY;
	if (sel & 0x08) opts |= CONFIG_OPT_SUBMIT_SYNTAX;

	MACRO_SET set;
	set.initialize(opts); // errors go into set.errors rather than to stderr
	set.defaults = &FuzzDefaults;

	MACRO_EVAL_CONTEXT ctx;
	ctx.init((sel & 0x10) ? "MASTER" : "TOOL", 0);
	if (sel & 0x20) ctx.localname = "LOCAL";
	ctx.cwd = "/fuzz";

	MACRO_SOURCE source{};
	insert_source("fuzz", set, source);

	MacroStreamMemoryFile ms(s.c_str(), s.size(), source);
	std::string errmsg;
	Parse_macros(ms, 0, set, opts, &ctx, errmsg, nullptr, nullptr);

	unsigned int expand_opts = 0;
	if (sel & 0x40) expand_opts |= EXPAND_MACRO_OPT_KEEP_DOLLARDOLLAR;
	if (sel & 0x80) expand_opts |= EXPAND_MACRO_OPT_IS_PATH;

	// Now expand everything that was defined, as param() would,
	// with both overloads of expand_macro, since their macro function code is separate
	HASHITER it = hash_iter_begin(set, HASHITER_NO_DEFAULTS);
	while ( ! hash_iter_done(it)) {
		const char * val = hash_iter_value(it);
		if (val) {
			free(expand_macro(val, set, ctx));
			std::string sval(val);
			expand_macro(sval, expand_opts, set, ctx);
		}
		hash_iter_next(it);
	}

	return 0;
}
