/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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

#pragma once

class BackwardFileReader {
protected:

	class BWReaderBuffer {

	protected:
		char * data;	 // buffer
		int    cbData;   // size of valid data in the buffer
		int    cbAlloc;	 // allocated size of the buffer
		bool   at_eof;   // set to true if the last read into the buffer went past end-of-file
		bool   text_mode; // true if the file was opened in text mode (text mode untested)
		int    error;    // holds value if errno when fread_at returns 0 and !at_eof

	public:
		BWReaderBuffer(int cb=0, char * input = NULL);
		~BWReaderBuffer() {
			if (data)
				free(data);
			data = NULL;
			cbData = cbAlloc = 0;
		}

		void clear() { cbData = 0; }
		int size() const { return cbData; }
		int capacity() const { return cbAlloc-1; }
		int LastError() const { return error; }
		bool AtEOF() const { return at_eof; }
		bool IsTextMode() const { return text_mode; }
		void SetTextMode(bool text) { text_mode = text; }
		char operator[](int ix) const { return data[ix]; }
		char& operator[](int ix) { return data[ix]; }

		void setsize(int cb);
		bool reserve(int cb);

		/* returns number of characters read. or 0 on error use LastError & AtEOF methods to know which.
		*/
		int fread_at(FILE * file, int64_t offset, int cb);
	};

	int error;	 // holds value of errno if there was a failure.
	FILE * file; // file we are reading from.
	filesize_t cbFile; // size of the file we are reading from
	int64_t     cbPos;  // location in the file that the buffer was read from.
	BWReaderBuffer buf; // buffer to help with backward reading.

public:
	BackwardFileReader(std::string filename, int open_flags);
	BackwardFileReader(int fd, const char * open_options);
	~BackwardFileReader() {
		if (file) fclose(file);
		file = NULL;
	}

	int  LastError() const { 
		return error; 
	}
	bool AtEOF() { 
		if ( ! file || (cbPos >= cbFile))
			return true; 
		return false;
	}
	bool AtBOF() { 
		if ( ! file || (cbPos == 0))
			return true; 
		return false;
	}
	void Close() {
		if ( file) fclose(file);
		file = NULL;
	}

	/*
	bool NextLine(std::string & str) {
		// write this
		return false;
	}
	*/

	bool PrevLine(std::string & str);

private:
	bool OpenFile(int fd, const char * open_options);

	// prefixes or part of a line into str, and updates internal
	// variables to keep track of what parts of the buffer have been returned.
	bool PrevLineFromBuf(std::string & str);

};
