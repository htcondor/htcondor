#include "classad/classad.h"
#include "classad/classad_distribution.h"

#include <string>
#include <string.h>
#include <sys/types.h>


/* This is the driver for the classad fuzz tester.
 * Note this is intentionally not in the cmake file, it should not be built
 * by default.
 *
 * To use, compile all of classads with clang -fsanitize=fuzzer-no-link,address
 * then recompile this one file with clang -fsanitize=fuzzer
 * This adds a magic main function here.
 * Then, just run ./a.out and let it run for a long time.
 */

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *input, size_t len) {

	classad::ClassAdParser parser;
	classad::ExprTree  *tree;
	std::string s(input, input + len);

	tree = parser.ParseExpression(s);
	if (tree) {
		classad::ClassAd ad;
		classad::Value val;
		ad.Insert("ExpressionToEvaluate", tree);
		ad.EvaluateAttr("ExpressionToEvaluate", val);
	}

	return 0;
}

