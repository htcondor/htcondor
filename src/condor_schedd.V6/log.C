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
LogRecord::readstring(int fd, char * &str)
{
	char	buf[1000];
	int		i;
	int		rval;

	for (i = 0; i < sizeof(buf) - 1; i++) {
		rval = read(fd, &(buf[i]), 1);
		if (rval < 0) {
			return rval;
		}
		if (buf[i] == '\0') {
			break;
		}
	}
	buf[i] = '\0';
	str = strdup(buf);
	return i;
}


int
LogRecord::Write(int fd)
{
	WriteHeader(fd);
	WriteBody(fd);
	WriteTail(fd);
}


int
LogRecord::Write(XDR *xdrs)
{
	xdrs->x_op = XDR_ENCODE;
	WriteHeader(xdrs);
	WriteBody(xdrs);
	WriteTail(xdrs);
}


int
LogRecord::Read(int fd)
{
	ReadHeader(fd);
	ReadBody(fd);
	ReadTail(fd);
}


int
LogRecord::Read(XDR *xdrs)
{
	xdrs->x_op = XDR_DECODE;

	ReadHeader(xdrs);
	ReadBody(xdrs);
	ReadTail(xdrs);
}


int
LogRecord::WriteHeader(int fd)
{
	int	rval, tot;

	rval = write(fd, &op_type, sizeof(op_type));
	if (rval < 0) {
		return rval;
	}
	tot = rval;
	rval = write(fd, &body_size, sizeof(body_size));
	if (rval < 0) {
		return rval;
	}
	tot += rval;
	return tot;
}


int LogRecord::WriteTail(int fd)
{
	int	rval, tot;

	rval = write(fd, &op_type, sizeof(op_type));
	if (rval < 0) {
		return rval;
	}
	tot = rval;
	rval = write(fd, &body_size, sizeof(body_size));
	if (rval < 0) {
		return rval;
	}
	tot += rval;
	return tot;
}


int
LogRecord::ReadHeader(int fd)
{
	int	rval, tot;

	rval = read(fd, &op_type, sizeof(op_type));
	if (rval < 0) {
		return rval;
	}
	tot = rval;
	rval = read(fd, &body_size, sizeof(body_size));
	if (rval < 0) {
		return rval;
	}
	tot += rval;
	return tot;
}


int LogRecord::ReadTail(int fd)
{
	int	rval, tot;

	rval = read(fd, &op_type, sizeof(op_type));
	if (rval < 0) {
		return rval;
	}
	tot = rval;
	rval = read(fd, &body_size, sizeof(body_size));
	if (rval < 0) {
		return rval;
	}
	tot += rval;
	return tot;
}


int
LogRecord::WriteHeader(XDR *xdrs)
{
	int	rval, tot;

	rval = xdr_int(xdrs, &op_type);
	if (!rval) {
		return rval;
	}
	rval = xdr_int(xdrs, &body_size);
	return rval;
}


int LogRecord::WriteTail(XDR *xdrs)
{
	return WriteHeader(xdrs);
}


int
LogRecord::ReadHeader(XDR *xdrs)
{
	return WriteHeader(xdrs);
}


int LogRecord::ReadTail(XDR *xdrs)
{
	return WriteTail(xdrs);
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
	head_only.ReadTail(fd);
	return log_rec;
}
