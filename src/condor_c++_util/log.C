/* 
** Copyright 1996 by Miron Livny, and Jim Pruyne
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Pruyne
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"

#include "log.h"

LogRecord::LogRecord()
{
}


LogRecord::~LogRecord()
{
}

int
LogRecord::readword(int fd, char * &str)
{
	char	buf[1000];
	int		i;
	int		rval;

	// ignore leading whitespace
	do {
		rval = read(fd, &(buf[0]), 1);
		if (rval <= 0) {
			return -1;
		}
	} while (isspace(buf[0]));

	// read until whitespace
	for (i = 1; i < sizeof(buf) - 1; i++) {
		rval = read(fd, &(buf[i]), 1);
		if (rval < 0) {
			return rval;
		}
		if (rval == 0) break;
		if (isspace(buf[i])) break;
		if (buf[i] == '\0') break;	// shouldn't get '\0'
	}
	buf[i] = '\0';
	str = strdup(buf);
	return i;
}

int
LogRecord::readline(int fd, char * &str)
{
	char	buf[1000];
	int		i;
	int		rval;

	// ignore leading whitespace
	do {
		rval = read(fd, &(buf[0]), 1);
		if (rval < 0) {
			return rval;
		}
	} while (isspace(buf[0]));

	// read until newline
	for (i = 1; i < sizeof(buf) - 1; i++) {
		rval = read(fd, &(buf[i]), 1);
		if (rval < 0) {
			return rval;
		}
		if (rval == 0) break;
		if (buf[i] == '\n') break;
		if (buf[i] == '\0') break;	// shouldn't get '\0'
	}
	buf[i] = '\0';
	str = strdup(buf);
	return i;
}

int
LogRecord::Write(int fd)
{
	int rval;

	rval = WriteHeader(fd);
	rval += WriteBody(fd);
	rval += WriteTail(fd);

	return rval;
}


int
LogRecord::Write(FILE *fp)
{
	return WriteHeader(fp) + WriteBody(fp) + WriteTail(fp);
}


int
LogRecord::Read(int fd)
{
	return ReadHeader(fd) + ReadBody(fd) + ReadTail(fd);
}


int
LogRecord::Read(FILE *fp)
{
	return ReadHeader(fp) + ReadBody(fp) + ReadTail(fp);
}


int
LogRecord::WriteHeader(int fd)
{
	char op[20];

	sprintf(op, "%d ", op_type);

	return write(fd, op, strlen(op));
}


int
LogRecord::WriteHeader(FILE *fp)
{
	return fprintf(fp, "%d ", op_type);
}


int
LogRecord::WriteTail(int fd)
{
	return write(fd, "\n", 1);
}


int
LogRecord::WriteTail(FILE *fp)
{
	return fprintf(fp, "\n");
}


int
LogRecord::ReadHeader(int fd)
{
	int	rval;
	char *op = NULL;

	rval = readword(fd, op);
	if (rval < 0) {
		return rval;
	}
	op_type = atoi(op);
	free(op);
	return rval;
}


int
LogRecord::ReadHeader(FILE *fp)
{
	return fscanf(fp, "%d ", &op_type);
}


int
LogRecord::ReadTail(FILE *fp)
{
	return 0;
}


int
LogRecord::ReadTail(int fd)
{
	return 0;
}


LogRecord *
ReadLogEntry(int fd)
{
	LogRecord		*log_rec;
	LogRecord		head_only;
	int				rval;

	rval = head_only.ReadHeader(fd);
	if (rval < 0) {
		return 0;
	}
	log_rec = InstantiateLogEntry(fd, head_only.get_op_type());
	if (head_only.ReadTail(fd) < 0) {
		delete log_rec;
		return 0;
	}
	return log_rec;
}
