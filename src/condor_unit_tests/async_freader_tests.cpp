/***************************************************************
 *
 * Copyright (C) 1990-2013, Red Hat Inc.
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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_random_num.h"

// override the EXCEPT macro inso we can test it.
#undef EXCEPT
#define EXCEPT(msg,...) {}
#include "my_async_fread.h"

#ifdef WIN32
#undef sleep
#define sleep(x) SleepEx(x*1000,TRUE)
#endif


int num_tests = 0;
int tests_failed = 0;
int total_fail_count = 0;
int step_fail_count = 0;
time_t test_begin_time = 0;
bool verbose = false;
bool diagnostic = false;
bool do_sync_io = false;

typedef struct _diagnostic_data {
	int cstatus_success;
	int cstatus_pending;
	int cstatus_queued;
	int cstatus_error;
	int last_error_status;
	int tot_reads;
	int tot_inprog;

	void clear() { memset(this, 0, sizeof(*this)); }
	void collect(MyAsyncFileReader &reader) {
		int status = reader.current_status();
		switch (status) {
		case 0: ++cstatus_success; break;
		case EINPROGRESS: ++cstatus_pending; break;
		case MyAsyncFileReader::READ_QUEUED: ++cstatus_queued; break;
		default: ++cstatus_error; last_error_status = status; break;
		}
		int tr=-1, tip=-1;
		reader.get_stats(tr, tip);
		tot_reads = MAX(tr, tot_reads);
		tot_inprog = MAX(tip, tot_inprog);
	}
	void report() {
		fprintf(verbose ? stdout : stderr, "\tlast status=%d, ok=%d, pend=%d, Q=%d, errs=%d, total reads=%d, inprog=%d\n",
		last_error_status, cstatus_success, cstatus_pending, cstatus_queued, cstatus_error, tot_reads, tot_inprog);
	}
} DIAGNOSTIC_DATA;

DIAGNOSTIC_DATA diag;

#define REQUIRE( condition ) \
	if(! ( condition )) { \
		if (verbose && ! step_fail_count) fprintf(stdout, "\n"); \
		fprintf( verbose ? stdout : stderr, "Failed %5d: %s\n", __LINE__, #condition ); \
		++step_fail_count; ++total_fail_count; \
	}

#define REQUIRE_EQUAL( val, ex, typ, fmt ) { \
	typ zzz = val; if( ! (zzz == (ex))) { \
		if (verbose && ! step_fail_count) fprintf(stdout, "\n"); \
		fprintf( verbose ? stdout : stderr, "Failed %5d: %s == %s (actual is " #fmt ")\n", __LINE__, #val, #ex, zzz ); \
		++step_fail_count; ++total_fail_count; \
	}}

#define BEGIN_TEST_CASE(fn) static void test_##fn(void) \
  { \
	const char * tag = #fn ; step_fail_count = 0; test_begin_time = time(NULL); ++num_tests; diag.clear(); \
	if (verbose) { fprintf(stdout, "## test %s : ", tag); fflush(stdout); }

#define END_TEST_CASE \
	if (verbose || step_fail_count) { \
		if (step_fail_count) { ++tests_failed; fprintf(verbose ? stdout : stderr, "\ntest %s failed %d steps\n", tag, step_fail_count); } \
		else fprintf(stdout, "succeeded\n"); \
		if (diagnostic || verbose) diag.report(); \
	} \
  }

#define CHECK(cond) REQUIRE(cond)
#define CHECK_EQUAL(value,result)     REQUIRE((value) == (result))
#define CHECK_EQUAL_INT(value,result) REQUIRE_EQUAL(value,result,int,"%d")
#define CHECK_TEST_TIMEOUT(sec) if ((time(NULL) - test_begin_time) >= (sec)) { REQUIRE((time(NULL) - test_begin_time) >= (sec)); break; }
#define CHECK_SYNC(r) if (do_sync_io) { REQUIRE(r.set_sync(true) == false); }

BEGIN_TEST_CASE(default_constructor) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;

	CHECK(reader.is_closed());
	CHECK( ! reader.eof_was_read());
	CHECK_EQUAL_INT(reader.error_code(), MyAsyncFileReader::NOT_INTIALIZED);
	CHECK( ! reader.get_data(p1, c1, p2, c2));
} END_TEST_CASE

BEGIN_TEST_CASE(10_lines) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_10_lines.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int lineno = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { ++lineno; }
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", lineno, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) { diag.report(); }
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(lineno, 10);
	// if (lineno != 10) { fprintf(stderr, "lineno = %d\n", lineno); }

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(no_newlines) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_no_newlines.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int lineno = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { ++lineno; }
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", lineno, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) { diag.report(); }
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(lineno, 1);
	//if (lineno != 1) { fprintf(stderr, "lineno = %d\n", lineno); }

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(no_data) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_no_data.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); }
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", cbdata, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) { diag.report(); }
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 0);
	// if (cbdata != 0) { fprintf(stderr, "cbdata = %d\n", cbdata); }

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(1byte) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_1byte.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	int nlines = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); ++nlines; }
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", cbdata, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) { diag.report(); }
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 1);
	CHECK_EQUAL_INT(nlines, 1);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(2bytes) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_2bytes.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	int nlines = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); ++nlines; }
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", cbdata, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) { diag.report(); }
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 2);
	CHECK_EQUAL_INT(nlines, 1);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE


BEGIN_TEST_CASE(1Kilobyte) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_1Kilobyte.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); }
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", cbdata, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) { diag.report(); }
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 1024);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE


BEGIN_TEST_CASE(2000_lines) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_2000_lines.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int lineno = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { ++lineno; }
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", lineno, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) diag.report();
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(lineno, 2000);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(2000_lines_again) {
	std::string str;
	MyAsyncFileReader reader;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_2000_lines.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int lineno = 0;
	for (;;) {
		if (reader.readline(str)) {
			++lineno;
			if (diagnostic) fprintf(stdout, "[%d] : %s\n", lineno, str.c_str());
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) diag.report();
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(lineno, 2000);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE


BEGIN_TEST_CASE(300000_bytes) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_300000_bytes.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); }
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) diag.report();
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 300000);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(1Megabyte) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_1Megabyte.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); }
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) diag.report();
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 1024*1024);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(1Megabyte_whole_file) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_1Megabyte.data", true), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			// once we get any data, we should have all of it.
			cbdata = c1 + (p2?c2:0);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) diag.report();
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 1024*1024);

	MyStringAioSource & src = reader.output();
	CHECK(src.allDataIsAvailable());

	// now read all of the data through the output method
	cbdata = 0;
	int lineno = 0;
	while (src.readLine(str)) { 
		++lineno;
		if (diagnostic) fprintf(stdout, "[%d] : %s\n", lineno, str.c_str());
		cbdata += str.size();
	}
	CHECK_EQUAL_INT(cbdata, 1024*1024);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

BEGIN_TEST_CASE(1Megabyte_no_final_newline) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_1Megabyte_no_final_newline.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); }
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) diag.report();
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 1024*1024);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE


BEGIN_TEST_CASE(30Megabytes) {
	MyAsyncFileReader reader;
	const char * p1, *p2;
	int c1, c2;
	std::string str;

	CHECK_SYNC(reader);
	CHECK_EQUAL_INT(reader.open("async_test_30Megabytes.data"), 0);
	CHECK_EQUAL_INT(reader.queue_next_read(), 0);

	int cbdata = 0;
	for (;;) {
		if (reader.get_data(p1, c1, p2, c2)) {
			if (reader.output().readLine(str, false)) { cbdata += str.size(); }
		} else if (reader.done_reading()) {
			diag.collect(reader);
			break;
		} else {
			diag.collect(reader);
			if (diagnostic) diag.report();
			sleep(0);
		}
		CHECK_TEST_TIMEOUT(60);
	}
	CHECK_EQUAL_INT(reader.error_code(), 0);
	CHECK_EQUAL_INT(cbdata, 30*1024*1024);

	CHECK(reader.is_closed());
	CHECK(reader.eof_was_read());
} END_TEST_CASE

// ---------------------------------------------
//     TESTFILE GENERATION
// ---------------------------------------------

#ifndef O_DIRECT
#define O_DIRECT 0
#endif

// simple class to fill a buffer and temporarily hold onto data that is too large for the buffer
// using it to initialize the buffer after reset and before any new data is written.
class bufholder {
protected:
	char * buf;
	int ix;
	int cbmax;
	const char * extra;
	int cbextra;
public:
	bufholder(char * b, int cb) : buf(b), ix(0), cbmax(cb), extra(0), cbextra(0) {}
	// append data to the buffer, returns true if there is still room in the buffer
	// after writing the data, false if there is not. 
	bool append(const char*b, int cb) {
		int cbremain = cbmax - ix;
		int cbadd = MIN(cb, cbremain);
		memcpy(buf+ix, b, cbadd);
		ix += cbadd;
		if (ix >= cbmax) {
			assert(ix == cbmax);
			if (cbadd < cb) {
				extra = b+cbadd;
				cbextra = cb - cbadd;
			}
			return false;
		}
		return true;
	}
	int align(int cb, int ch) {
		while (ix % cb) {
			buf[ix] = ch;
			++ix;
		}
		return ix;
	}
	void reset() {
		ix = 0;
		if (extra) append(extra, cbextra);
		extra = NULL;
		cbextra = 0;
	}
	const void * get_data(int & cb) { cb = ix; return ix ? buf : NULL; }
};

int generate_file(const char *filename, const char * dir, char* buf, int bufsize, const char * (*generate)(void *inst, int step, int cbtot, int &cb), void*inst)
{
	std::string fullpath;
	if (dir) { 
		formatstr(fullpath, "%s%c%s", dir, DIR_DELIM_CHAR, filename);
		filename = fullpath.c_str();
	}

	fprintf(stdout, "creating test file '%s' ", filename);

	bufholder bh(buf, bufsize);
	int rval = 0;

#ifdef WIN32
	HANDLE fd = CreateFile(filename, GENERIC_WRITE, 0, 0,
		CREATE_ALWAYS,
		FILE_FLAG_NO_BUFFERING,
		NULL);
#else
	int fd = open(filename, O_CREAT | _O_BINARY | O_DIRECT | O_WRONLY, 0664);
#endif

	int cbtot = 0;
	int ixpos = 0;
	for (int step = 0; ; ++step) {
		int cb, cbwrote;
		const char * data = generate(inst, step, ixpos, cb);
		if ( ! data)
			break;

		ixpos += cb;

		if ( ! bh.append(data, cb)) {
			const void * pv = bh.get_data(cb);
		#ifdef WIN32
			DWORD err;
			if ( ! WriteFile(fd, pv, cb, (DWORD*)&cbwrote, NULL)) {
				err = GetLastError();
				fprintf(stdout, "Write failed %d != %d, err=%d\n", cb, cbwrote, err);
		#else
			int err;
			cbwrote = (int)write(fd, pv, cb);
			if (cbwrote != cb) {
				err = errno;
				fprintf(stdout, "Write failed %d != %d, err=%d %s\n", cb, cbwrote, err, strerror(err));
		#endif
				rval = -1;
				break;
			}

			bh.reset();
		}
	}

	int cb=0, cbwrote=0;
	const void * pv = bh.get_data(cb);
	if (pv && cb > 0) {
		cbtot += cb;
		// force cb to be block aligned.
		int cbwrite = bh.align(0x1000, '~');
		fprintf(stdout, "final write, cb=%d, cbwrite=%d\n", cb, cbwrite);
	#ifdef WIN32
		DWORD err;
		if ( ! WriteFile(fd, pv, cbwrite, (DWORD*)&cbwrote, NULL)) {
			err = GetLastError();
			fprintf(stdout, "\nWrite of final data block failed %d!=%d, err=%d\n", cbwrite, cbwrote, err);
	#else
		int err;
		cbwrote = (int)write(fd, pv, cbwrite);
		if (cbwrote != cbwrite) {
			err = errno;
			fprintf(stdout, "\nWrite of final data block failed %d!=%d, err=%d %s\n", cbwrite, cbwrote, err, strerror(err));
	#endif
			rval = -1;
		}

		// now set the file size to the desired actual file size
		if (cbwrite > cb) {
			fprintf(stderr, "Truncating file to cb=%d\n", cbtot);
		#ifdef WIN32
			CloseHandle(fd);
			// we have to re-open the file with buffering in order to set the data length to something not page aligned (sigh)
			fd = CreateFile(filename, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			SetFilePointer(fd, cbtot, NULL, SEEK_SET);
			SetEndOfFile(fd);
		#else
			if (ftruncate(fd, cbtot) == -1) {
				fprintf(stdout, "ftrucate failed %d %s\n", errno, strerror(errno));
			}
		#endif
		}
	}

	#ifdef WIN32
		CloseHandle(fd);
	#else
		close(fd);
	#endif

	return rval;
}

struct line_generator_control {
	std::string line;
	int max_lines;
	int max_bytes;
	int max_line_length;
	bool last_line_has_newline;
};

// returns a line of randomly generated length with terminating newline
// the line will not exceed the line length indicated by the control structure
// also, the total size of all the lines and the number of lines will not exceed
// the limits indicated in the control structure.
const char * random_line_generator(void* inst, int step, int cbtot, int & cb)
{
	struct line_generator_control &ctl = *((struct line_generator_control*)inst);
	ctl.line.clear();

	// limit the number of lines in the file.
	if (step >= ctl.max_lines || cbtot >= ctl.max_bytes) {
		cb = 0;
		return NULL;
	}
	int remain = ctl.max_bytes - cbtot;

	int ch = (rand() % ('z' - ' ')) + ' ';
	int len = 1 + rand() % ctl.max_line_length;

	// limit the file size. we may or may not want to make sure that the last line ends in a newline.
	len = MIN(remain, len);
	ctl.line.replace(0, 0, len, ch);
	if (remain > len || ctl.last_line_has_newline) {
		ctl.line.replace(len-1,1,1,'\n');
	}
	cb = len;
	return ctl.line.data();
}

const char * literal_line_generator(void* inst, int step, int /*cbtot*/, int &cb)
{
	if (step > 0) { cb = 0; return NULL; }
	const char * line = (const char *)inst;
	cb = (int)strlen(line);
	return line;
}

// generate the files needed to test with.
int generate_files(const char * dir)
{
	// we will use a write buffer that is aligned to a 1mb boundary to make sure that direct io works correctly

	// make sure we have repeatable random numbers for the random part of the test.
	srand(777);

	const int bufsize = 0x1000 * 0x1000;
	ssize_t pv = (ssize_t)malloc(bufsize*2); // allocate a 2x buffer so we can guarantee aligment
	char * buf = (char*)((pv+bufsize-1) & ~(bufsize-1)); // align to even multiple of bufsize
	auto_free_ptr buf_holder((char *)pv);

#if 1

	struct line_generator_control ctl;
	ctl.last_line_has_newline = true;
	ctl.max_bytes = INT_MAX;
	ctl.max_lines = 10;
	ctl.max_line_length = 1280;
	int rval = generate_file ("async_test_10_lines.data", dir, buf, bufsize, random_line_generator, &ctl);
	if (rval) return rval;

	char filedata_no_newlines[] = "??????????";
	rval = generate_file ("async_test_no_newlines.data", dir, buf, bufsize, literal_line_generator, filedata_no_newlines);
	if (rval) return rval;

	ctl.last_line_has_newline = true;
	ctl.max_bytes = INT_MAX;
	ctl.max_lines = 2000;
	ctl.max_line_length = 120;
	rval = generate_file ("async_test_2000_lines.data", dir, buf, bufsize, random_line_generator, &ctl);
	if (rval) return rval;

	// make a file that is 0 kb in size
	ctl.last_line_has_newline = false;
	ctl.max_bytes = 0;
	ctl.max_lines = 0;
	ctl.max_line_length = 0;
	rval = generate_file ("async_test_no_data.data", dir, buf, bufsize, random_line_generator, &ctl);
	if (rval) return rval;

	// make a file that is 1 byte in size
	char filedata_one_byte[] = "\n";
	rval = generate_file ("async_test_1byte.data", dir, buf, bufsize, literal_line_generator, filedata_one_byte);
	if (rval) return rval;

	// make a file that is 2 bytes in size
	char filedata_two_bytes[] = "a\n";
	rval = generate_file ("async_test_2bytes.data", dir, buf, bufsize, literal_line_generator, filedata_two_bytes);
	if (rval) return rval;

	// make a file that is 1k in size
	ctl.last_line_has_newline = true;
	ctl.max_bytes = 1024;
	ctl.max_lines = INT_MAX;
	ctl.max_line_length = 128;
	rval = generate_file ("async_test_1Kilobyte.data", dir, buf, bufsize, random_line_generator, &ctl);
	if (rval) return rval;

	// make a file that is 300,000 bytes in size
	ctl.last_line_has_newline = true;
	ctl.max_bytes = 300000;
	ctl.max_lines = INT_MAX;
	ctl.max_line_length = 120;
	rval = generate_file ("async_test_300000_bytes.data", dir, buf, bufsize, random_line_generator, &ctl);
	if (rval) return rval;

	// make a file that is 1 Mb bytes in size
	ctl.last_line_has_newline = true;
	ctl.max_bytes = 1024*1024;
	ctl.max_lines = INT_MAX;
	ctl.max_line_length = 112;
	rval = generate_file ("async_test_1Megabyte.data", dir, buf, bufsize, random_line_generator, &ctl); 
	if (rval) return rval;
	// make a file that is 1 Mb bytes in size but the file does not end with a newline
	ctl.last_line_has_newline = false;
	rval = generate_file ("async_test_1Megabyte_no_final_newline.data", dir, buf, bufsize, random_line_generator, &ctl); 
	if (rval) return rval;

	// make a file that is 30 Mb bytes in size
	ctl.last_line_has_newline = true;
	ctl.max_bytes = 1024*1024*30;
	ctl.max_lines = INT_MAX;
	ctl.max_line_length = 1120;
	rval = generate_file ("async_test_30Megabytes.data", dir, buf, bufsize, random_line_generator, &ctl); 
	if (rval) return rval;

#else
	std::string tmp;

	// make a 10 line file with lines at most 1280 characters in length (including the newline)
	//
	int rval = generate_file (
		"async_test_10_lines.data",
		dir, buf, bufsize,
		[](void* inst, int step, int /*cbtot*/, int & cb) -> const char * {

			// make a 2000 line file with lines at most 120 characters in length.
			if (step >= 10) {
				cb = 0;
				return NULL;
			}

			std::string &line = *((std::string *)inst);
			line.clear();

			int ch = (rand() % ('z' - ' ')) + ' ';
			int len = rand() % 1279;
			if (len > 0) { line.replace(0, 0, len, ch); }
			line += '\n';

			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a 10 line file with lines at most 1280 characters in length (including the newline)
	//
	rval = generate_file (
		"async_test_no_newlines.data",
		dir, buf, bufsize, 
		[](void* inst, int step, int /*cbtot*/, int & cb) -> const char * {

			// make a 1 line file containg 10
			if (step > 1) {
				cb = 0;
				return NULL;
			}

			std::string &line = *((std::string *)inst);
			line.clear();
			line.replace(0, 0, 10, '?');
			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a 2000 line file with lines at most 120 characters in length (including the newline)
	//
	rval = generate_file (
		"async_test_2000_lines.data",
		dir, buf, bufsize, 
		[](void* inst, int step, int /*cbtot*/, int & cb) -> const char * {

			// make a 2000 line file with lines at most 120 characters in length.
			if (step >= 2000) {
				cb = 0;
				return NULL;
			}

			std::string &line = *((std::string *)inst);
			line.clear();

			int ch = (rand() % ('z' - ' ')) + ' ';
			int len = rand() % 119;
			if (len > 0) { line.replace(0, 0, len, ch); }
			line += '\n';

			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a file that is 0 kb in size
	rval = generate_file (
		"async_test_no_data.data",
		dir, buf, bufsize, 
		[](void* inst, int step, int /*cbtot*/, int & cb) -> const char * {

			// make an empty file.
			cb = 0;
			if (step > 0) return NULL;
			std::string &line = *((std::string *)inst);
			line = "";
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a file that is 1 byte in size
	rval = generate_file (
		"async_test_1byte.data",
		dir, buf, bufsize, 
		[](void* inst, int step, int /*cbtot*/, int & cb) -> const char * {

			// make a 1 byte file
			cb = 0;
			if (step > 0) return NULL;
			std::string &line = *((std::string *)inst);
			line = "\n";
			cb = 1;
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a file that is 1 byte in size
	rval = generate_file (
		"async_test_2bytes.data",
		dir, buf, bufsize, 
		[](void* inst, int step, int /*cbtot*/, int & cb) -> const char * {

			// make a 1 byte file
			cb = 0;
			if (step > 0) return NULL;
			std::string &line = *((std::string *)inst);
			line = "a\n";
			cb = 2;
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a file that is 1k in size
	rval = generate_file (
		"async_test_1Kilobyte.data",
		dir, buf, bufsize, 
		[](void* inst, int /*step*/, int cbtot, int & cb) -> const char * {

			// make a 1Kb file
			const int maxfile = 1024;
			if (cbtot >= maxfile) {
				cb = 0;
				return NULL;
			}

			std::string &line = *((std::string *)inst);
			line.clear();

			int remain = maxfile - cbtot;
			int maxline = MIN(remain-1, 127); // maximum line length -1 to account for \n
			if (maxline > 0) {
				int ch = (rand() % ('z' - ' ')) + ' ';
				int len = 1 + rand() % maxline;
				line.replace(0, 0, len, ch);
			}
			line += '\n';

			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;


	// make a file that is 300,000 bytes in size
	rval = generate_file (
		"async_test_300000_bytes.data",
		dir, buf, bufsize, 
		[](void* inst, int /*step*/, int cbtot, int & cb) -> const char * {

			// make a 300000 line file with lines at most 120 characters in length.
			const int maxfile = 300000;
			if (cbtot >= maxfile) {
				cb = 0;
				return NULL;
			}

			std::string &line = *((std::string *)inst);
			line.clear();

			int remain = maxfile - cbtot;
			int maxline = MIN(remain-1, 119); // maximum line length -1 to account for \n
			if (maxline > 0) {
				int ch = (rand() % ('z' - ' ')) + ' ';
				int len = 1 + rand() % maxline;
				line.replace(0, 0, len, ch);
			}
			line += '\n';

			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a file that is 1 Mb bytes in size
	rval = generate_file (
		"async_test_1Megabyte.data",
		dir, buf, bufsize, 
		[](void* inst, int /*step*/, int cbtot, int & cb) -> const char * {

			// make a 3Mb file with lines at most 1200 characters in length.
			const int maxfile = 1024 * 1024;
			if (cbtot >= maxfile) {
				cb = 0;
				return NULL;
			}
			std::string &line = *((std::string *)inst);
			line.clear();

			int remain = maxfile - cbtot;
			int maxline = MIN(remain-1, 111); // maximum line length -1 to account for \n
			if (maxline > 0) {
				int ch = (rand() % ('z' - ' ')) + ' ';
				int len = 1 + rand() % maxline;
				line.replace(0, 0, len, ch);
			}
			line += '\n';

			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a file that is 1 Mb bytes in size but the file does not end with a newline
	rval = generate_file (
		"async_test_1Megabyte_no_final_newline.data",
		dir, buf, bufsize, 
		[](void* inst, int /*step*/, int cbtot, int & cb) -> const char * {

			// make a 3Mb file with lines at most 1200 characters in length.
			const int maxfile = 1024 * 1024;
			if (cbtot >= maxfile) {
				cb = 0;
				return NULL;
			}
			std::string &line = *((std::string *)inst);
			line.clear();

			int remain = maxfile - cbtot;
			int maxline = MIN(remain, 111); // maximum line length -1 to account for \n
			if (maxline > 0) {
				int ch = (rand() % ('z' - ' ')) + ' ';
				int len = 1 + rand() % maxline;
				line.replace(0, 0, len, ch);
			}
			if (maxline < remain) {
				line += "\n";
			}
			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;

	// make a file that is 30 Mb bytes in size
	rval = generate_file (
		"async_test_30Megabytes.data",
		dir, buf, bufsize, 
		[](void* inst, int /*step*/, int cbtot, int & cb) -> const char * {

			// make a 3Mb file with lines at most 1200 characters in length.
			const int maxfile = 30 * 1024 * 1024;
			if (cbtot >= maxfile) {
				cb = 0;
				return NULL;
			}
			std::string &line = *((std::string *)inst);
			line.clear();

			int remain = maxfile - cbtot;
			int maxline = MIN(remain-1, 1119); // maximum line length -1 to account for \n
			if (maxline > 0) {
				int ch = (rand() % ('z' - ' ')) + ' ';
				int len = 1 + rand() % maxline;
				line.replace(0, 0, len, ch);
			}
			line += '\n';

			cb = line.size();
			return line.data();
		},
		(void*)&tmp);
	if (rval) return rval;
#endif

	return 0;
}

int main(int argc, const char * argv[])
{
	bool generate_test_files = false;
	const char * test_files_dir = NULL;
	for (int ixarg = 1; ixarg < argc; ++ixarg) {
		if (YourString(argv[ixarg]) == "-generate-test-files") {
			generate_test_files = true;
		}
		else
		if (YourString(argv[ixarg]) == "-dir") {
			test_files_dir = argv[++ixarg];
			if ( ! test_files_dir) {
				fprintf(stderr, "-dir requires an argument\n");
				return 1;
			}
		}
		else
		if (YourString(argv[ixarg]) == "-v") {
			verbose = true;
		}
		else
		if (YourString(argv[ixarg]) == "-diagnostic") {
			diagnostic = verbose = true;
		}
		else
		if (YourString(argv[ixarg]) == "-sync") {
			do_sync_io = true;
		}
		else
		{
			fprintf(stderr, "unrecognised argument '%s'\n", argv[ixarg]);
			return 1;
		}
	}

	if (generate_test_files) {
		return generate_files(test_files_dir);
	}

	// run the tests
	//
	test_default_constructor();
	test_10_lines();
	test_1byte();
	test_2bytes();
	test_2000_lines();
	test_2000_lines_again();
	test_1Kilobyte();
	test_300000_bytes();
	test_1Megabyte();
	test_1Megabyte_whole_file();
	test_30Megabytes();
	test_no_newlines();
	test_1Megabyte_no_final_newline();
	test_no_data();

	if ( ! tests_failed) {
		fprintf(stdout, "all %d tests pass\n", num_tests);
	} else {
		fprintf(stdout, "Out of %d tests, %d failed a total %d test steps\n", num_tests, tests_failed, total_fail_count);
	}
	return tests_failed;
}
