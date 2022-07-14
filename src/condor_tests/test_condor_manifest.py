#!/usr/bin/env pytest

import pytest

import subprocess
import tempfile
from pathlib import Path

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class Test_GetNumberFromFileName:

    TEST_CASES = [
        ( 'failure.z',          -1 ),
        ( 'failure.000',        -1 ),
        ( 'failure.0001',       -1 ),
        ( 'MANIFEST.z',         -1 ),
        ( 'MANIFEST.000',        0 ),
        ( 'MANIFEST.0001',       1 ),
        ( 'MANIFEST.0101',     101 ),
        ( 'MANIFEST.-101',      -1 ),
    ]

    @pytest.fixture(params=TEST_CASES, ids=[case[0] for case in TEST_CASES])
    def test_case(self, request):
        return (request.param[0], request.param[1])

    @pytest.fixture
    def expected(self, test_case):
        return test_case[1]

    @pytest.fixture
    def result(self, test_case):
        rv = subprocess.run(["condor_manifest", "getNumberFromFileName", test_case[0] ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=20)
        return int(rv.stdout.strip())

    def test(self, expected, result):
        assert expected == result


def writeToFile(path, contents):
    targetFile = tempfile.NamedTemporaryFile(
        mode="w", dir=path.as_posix(), delete=False
    )
    targetFile.write(contents)
    targetFile.flush()
    return targetFile.name

def validButUseless(test_dir):
    pathToManifestDir = test_dir / "validButUseless"
    pathToManifestDir.mkdir(parents=True, exist_ok=True)
    pathToManifestDir = Path(tempfile.mkdtemp(dir=pathToManifestDir))

    return makeManifestForDirectory(pathToManifestDir)

def emptyOtherFile(test_dir):
    pathToManifestDir = test_dir / "emptyOtherFile"
    pathToManifestDir.mkdir(parents=True, exist_ok=True)
    pathToManifestDir = Path(tempfile.mkdtemp(dir=pathToManifestDir))

    ( pathToManifestDir / "emptyOtherFile" ).touch()

    return makeManifestForDirectory(pathToManifestDir)

def singleOtherFile(test_dir):
    pathToManifestDir = test_dir / "singleOtherFile"
    pathToManifestDir.mkdir(parents=True, exist_ok=True)
    pathToManifestDir = Path(tempfile.mkdtemp(dir=pathToManifestDir))

    ( pathToManifestDir / "singleOtherFile" ).write_text("singleOtherFile")

    return makeManifestForDirectory(pathToManifestDir)

def theComplicatedOne(test_dir):
    pathToManifestDir = test_dir / "theComplicatedOne"
    pathToManifestDir.mkdir(parents=True, exist_ok=True)
    pathToManifestDir = Path(tempfile.mkdtemp(dir=pathToManifestDir))

    ( pathToManifestDir / "a" ).write_text("a")
    ( pathToManifestDir / "b" ).write_text("b")
    ( pathToManifestDir / "c" ).write_text("c")
    ( pathToManifestDir / "d" ).mkdir(parents=True, exist_ok=True)
    ( pathToManifestDir / "d" / "a" ).write_text("aa")
    ( pathToManifestDir / "d" / "b" ).mkdir(parents=True, exist_ok=True)
    ( pathToManifestDir / "d" / "b" / "a" ).mkdir(parents=True, exist_ok=True)
    ( pathToManifestDir / "d" / "b" / "a" / "a" ).write_text("aaa")
    ( pathToManifestDir / "d" / "b" / "b" ).write_text("bb")
    ( pathToManifestDir / "d" / "b" / "c" ).write_text("cc")
    ( pathToManifestDir / "d" / "c" ).write_text("ccc")
    ( pathToManifestDir / "d" / "d" ).mkdir(parents=True, exist_ok=True)
    ( pathToManifestDir / "d" / "d" / "a" ).write_text("aaaa")
    ( pathToManifestDir / "d" / "d" / "b" ).write_text("bbb")
    ( pathToManifestDir / "e" ).mkdir(parents=True, exist_ok=True)
    ( pathToManifestDir / "e" / "a" ).write_text("aaaaa")
    ( pathToManifestDir / "e" / "b" ).write_text("bbbb")
    ( pathToManifestDir / "e" / "c" ).mkdir(parents=True, exist_ok=True)
    ( pathToManifestDir / "e" / "d" ).symlink_to(( pathToManifestDir / "c" ))

    return makeManifestForDirectory(pathToManifestDir)

def mutateManifestHash(pathToManifestFile):
    path = Path(pathToManifestFile)
    contents = path.read_text().splitlines()
    # This actually tests the parser, not the hasher.
    contents[-1] = '!' + contents[-1][1:]
    path.write_text("\n".join(contents) + "\n")
    logger.debug(f"Mutated manifest hash in '{pathToManifestFile}'");
    return pathToManifestFile

def mutateFileHash(pathToManifestFile):
    path = Path(pathToManifestFile)
    contents = path.read_text().splitlines()
    # Swap the first two bytest to make sure we'r testing the hasher.
    contents[0] = contents[0][1] + contents[0][0] + contents[0][2:]
    path.write_text("\n".join(contents) + "\n")
    logger.debug(f"Mutated file hash in '{pathToManifestFile}'");
    return pathToManifestFile

def mutateFilePath(pathToManifestFile):
    path = Path(pathToManifestFile)
    contents = path.read_text().splitlines()
    contents[0] = contents[0][0:-1] + "!"
    path.write_text("\n".join(contents) + "\n")
    logger.debug(f"Mutated file path in '{pathToManifestFile}'");
    return pathToManifestFile

def sha256sum(pathToFile):
    args = ["sha256sum", "--binary", pathToFile.name]
    rv = subprocess.run( args,
            cwd=pathToFile.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20)
    assert(rv.returncode == 0)
    return rv.stdout

def makeManifestForDirectoryWith(pathToManifestDir, args, suffix=None):
    manifestText = mmfdw_impl(pathToManifestDir, pathToManifestDir, args, suffix)

    manifestFileName = "MANIFEST.0000"
    if suffix is not None:
        manifestFileName += suffix
    pathToPartialManifest = pathToManifestDir / manifestFileName
    pathToPartialManifest.write_bytes(manifestText)

    lastManifestLine = sha256sum(pathToPartialManifest)
    # s/MANIFEST.0000{suffix}/MANIFEST.0000
    if suffix is not None:
        lastManifestLine = lastManifestLine[:-(len(suffix) + 1)] + b'\n'

    pathToPartialManifest.write_bytes(manifestText + lastManifestLine)
    return pathToPartialManifest.as_posix()

def mmfdw_impl(pathToManifestDir, prefix, args, suffix):
    manifestText = b''

    # The semantics for HTCondor file transfer are that symlinks to files
    # copy the target, and that symlinks to directories put the job on hold.
    #
    # This code therefore just ignores symlinks to directories, although
    # it should never see any.
    for pathToFile in pathToManifestDir.iterdir():
        if pathToFile.name == "MANIFEST.0000":
            continue

        if pathToFile.is_block_device():
            continue
        if pathToFile.is_char_device():
            continue
        if pathToFile.is_dir():
            if not pathToFile.is_symlink():
                manifestText += mmfdw_impl(pathToFile, prefix, args, suffix)
            continue
        if pathToFile.is_fifo():
            continue

        relativePath = pathToFile.relative_to(prefix)
        rv = subprocess.run( args + [relativePath.as_posix()],
                cwd=prefix,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=20)
        assert(rv.returncode == 0)
        manifestText += rv.stdout

    return manifestText

def makeManifestForDirectory(pathToManifestDir):
    args = [ 'condor_manifest', 'compute_file_sha256_checksum' ]
    return makeManifestForDirectoryWith(pathToManifestDir, args)

class TestManifestFiles:

    STRING_TEST_CASES = [
        # The empty file should fail to validate itself and other files,
        # no matter how many lines it may or may not have.
        ( "",                               1,  1 ),
        ( "\n",                             1,  1 ),
        ( "\n\n",                           1,  1 ),
        ( "\n\n\n",                         1,  1 ),

        # Arbitrary junk shouldn't work, either.
        ( "9876 abcdef\n",                  1,  1 ),
        ( "abcd 1234 @$%^\n",               1,  1 ),
    ]

    # It's annoying but probably not worth hacking my way around that
    # PyTest runs the test cases sorted by function first, rather than
    # alternating between functions for each test case.
    FILE_TEST_CASES = [
        # A MANIFEST file which does not exist.
        ( lambda p: p / "missing",                              1, 1 ),

        # A MANIFEST describing only itself can be validated but not
        # used for validation.
        ( lambda p: validButUseless(p),                         0, 1 ),
        ( lambda p: mutateManifestHash(validButUseless(p)),     1, 1 ),

        # A MANIFEST describing only one other file, which is empty.
        ( lambda p: emptyOtherFile(p),                          0, 0 ),
        ( lambda p: mutateManifestHash(emptyOtherFile(p)),      1, 0 ),
        ( lambda p: mutateFileHash(singleOtherFile(p)),         1, 1 ),
        ( lambda p: mutateFilePath(singleOtherFile(p)),         1, 1 ),

        # A MANIFEST describing only one other file.
        ( lambda p: singleOtherFile(p),                         0, 0 ),
        ( lambda p: mutateManifestHash(singleOtherFile(p)),     1, 0 ),
        ( lambda p: mutateFileHash(singleOtherFile(p)),         1, 1 ),
        ( lambda p: mutateFilePath(singleOtherFile(p)),         1, 1 ),

        # There's a bunch of intermediate-complexity tests we could
        # before the complicated one, but there's no point writing
        # them unless this one fails and we can't figure out why.
        ( lambda p: theComplicatedOne(p),                       0, 0 ),
        ( lambda p: mutateManifestHash(theComplicatedOne(p)),   1, 0 ),
        ( lambda p: mutateFileHash(theComplicatedOne(p)),       1, 1 ),
        ( lambda p: mutateFilePath(theComplicatedOne(p)),       1, 1 ),

        # Test convolutions of the mutations.
        ( lambda p: mutateFileHash(mutateManifestHash(theComplicatedOne(p))), 1, 1 ),
        ( lambda p: mutateFilePath(mutateManifestHash(theComplicatedOne(p))), 1, 1 ),
        ( lambda p: mutateFileHash(mutateFilePath(theComplicatedOne(p))), 1, 1 ),
    ]

    TEST_CASES = [
        (lambda p: writeToFile(p, string_case[0]), string_case[1], string_case[2])
        for string_case in STRING_TEST_CASES
    ] + FILE_TEST_CASES

    @pytest.fixture(params=TEST_CASES)
    def test_case(self, request, test_dir):
        case = request.param
        return (case[0](test_dir), case[1], case[2])

    @pytest.fixture
    def file_expected(self, test_case):
        return test_case[1]

    @pytest.fixture
    def file_result(self, test_case):
        rv = subprocess.run([
                    "condor_manifest",
                    "validateManifestFile",
                    test_case[0],
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=20)
        return rv.returncode

    def test_validateManifestFile(self, test_case, file_expected, file_result):
        assert file_expected == file_result

    @pytest.fixture
    def list_expected(self, test_case):
        return test_case[2]

    @pytest.fixture
    def list_result(self, test_case):
        rv = subprocess.run(
                ["condor_manifest", "validateFilesListedIn", test_case[0] ],
                cwd = Path(test_case[0]).parent,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=20
        )
        return rv.returncode

    def test_validateFilesListedIn(self, test_case, list_expected, list_result):
        assert list_expected == list_result

    CRYPTO_TEST_CASES = [
        ( lambda p: validButUseless(p),                         0, 1 ),
        ( lambda p: emptyOtherFile(p),                          0, 0 ),
        ( lambda p: singleOtherFile(p),                         0, 0 ),
        ( lambda p: theComplicatedOne(p),                       0, 0 ),
    ]

    @pytest.fixture(params=CRYPTO_TEST_CASES)
    def crypto_test_case(self, request, test_dir):
        case = request.param
        return (case[0](test_dir), case[1], case[2])

    @pytest.fixture
    def crypto_result(self, crypto_test_case):
        return Path(crypto_test_case[0]).read_bytes()

    @pytest.fixture
    def crypto_expected(self, crypto_test_case):
        args = ["sha256sum", "--binary"]
        pathToManifestDir = Path(crypto_test_case[0]).parent
        return Path(makeManifestForDirectoryWith(
          pathToManifestDir, args, '.sha256sum')
        ).read_bytes()

    def test_compute_file_sha256_checksum(self, crypto_test_case, crypto_expected, crypto_result):
        assert crypto_expected == crypto_result
