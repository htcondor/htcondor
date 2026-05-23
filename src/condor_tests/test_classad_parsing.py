#!/usr/bin/env pytest

import io
import pytest

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import socket

from pytest_httpserver import HTTPServer

from ornithology import (
    action,
)

import classad2


@action
def http_server():
    with HTTPServer() as httpserver:
        httpserver.expect_request("/classad").respond_with_data('x = "ffo"')
        yield httpserver


def unseekable_classad_stream(http_server):
    s = socket.socket()
    s.settimeout(1)
    s.connect(('localhost', http_server.port))
    s.send(b'GET /classad HTTP/1.0\r\n\r\n')
    f = s.makefile(mode='rw')
    while True:
        line = f.readline()
        if line == "\n":
            break
        assert line != ""
    return f


@action
def ucsa(http_server):
    return unseekable_classad_stream(http_server)


@action
def ucsb(http_server):
    return unseekable_classad_stream(http_server)


@action
def ucsc(http_server):
    return unseekable_classad_stream(http_server)


@action
def ucsd(http_server):
    return unseekable_classad_stream(http_server)


class TestClassAdParsing:

    def test_unseekable_streams(self, http_server, ucsa, ucsb, ucsc, ucsd):
        c = classad2.parseOne(ucsa)
        assert c['x'] == 'ffo'

        d = list(classad2.parseAds(ucsb))[0]
        assert d['x'] == 'ffo'

        with pytest.raises(classad2.ClassAdException):
            e = classad2.parseNext(ucsc)
            assert(False)

        # I would prefer to verify that parseNext() parses properly,
        # but sockets don't support `fileno()`, and the whole point
        # of doing this with sockets is that I didn't want to have to
        # fork in order to test it with stdin.
        with pytest.raises(io.UnsupportedOperation, match="fileno"):
            f = classad2.parseNext(ucsd, classad2.ParserType.Old)
            assert f['x'] == 'ffo'


    def test_empty_ads(self):
        pass
        # We'll generate the string first, then we can feed it in via
        # the HTTP server, as above, for parseAds().
        #
        # That won't work for parseNext(), but at least we can re-use
        # the string.
        #
        # The string will have to be a set of strings (parameterized...)
        # where... actually, I think just calling list() and matching the
        # resulting sequence would work.
        #
        # (a) empty (b) empty / full (c) full / empty
        # (d) full / empty / full (e) empty / empty
        # (f) full / full / empty / empty / full / full / empty empty
        #
