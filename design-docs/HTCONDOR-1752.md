## Allow `htcondor jobsubset submit` to iterate over S3 objects

The `htcondor jobset submit` command generates sets of similar jobs by
using one or more specified job submit files as templates.  Variables
used but not set in the template are the blanks which must be filled in
to generate a job.  The use specifies an "iterator" to generate the list
of sets of variables; the length of the list determines how many jobs
are created from each template in the job set description file.

Presently, the only iterator type is `table`, which functions almost
identically to the `queue from` command in the job description language:
the user supplies an explicit commas-and-newlines table, each row of
which defines a set of variables for a single job.  The job description
language also offers `queue matching file`, which effectively generates
a single-column table, one row per (matching) file name.

### Goal

The goal is add a new iterator to `htcondor jobset submit` which functions
like `queue matching file` but for an S3 bucket.

### Specifications

We propose the following syntax --
```
iterator = bucket var-name bucket-url token-file token-file [token-file]
```
-- where *var-name* is the name of the variable that will iterate over
the objects in the bucket, and *bucket-url* is the URL of the bucket in question.
This retains the same general ordering as the `table` iterator type, where
the variable names precede the file name or table data.

For now, we'll require that *bucket-url* is an `https` URL of the form
`https://s3-server-name/bucket-name/object-prefix`, where *object-prefix* may
contain additional forward slashes.  We could in the future add the ability to
specify globs, although specifying globs in the bucket root could be very
expensive (or require assuming that object names are path-like for performance
reasons).

We further propose that *var-name* contain the whole S3 URL, both so that it
saves the submittor a bunch of typing and to make sure that it triggers the
built-in support for S3 URLs in the file-transfer object.

#### Security

If would be consistent with other uses of S3 in HTCondor for the security
tokens to be specified in their own attributes in the job set description
file, bu we instead propose to specify the token file names in the iterator
arguments for implementation convenience.

The implementation should probably propogate the security tokens to the
appropriate values in each job in the job set, although there's presently
no support for that.

### Implementation

The `iterator` keyword already requires an *iterator-type* argument, so it
will be fairly trivial to add a `bucket` type.

Given that the current iterator is implemented client-side, we propose a
client-side iterator for S3 buckets, as well.  A client-side iterator has
a number of advantages:

*  Early error checking and better error-reporting.  It's a little sad that
   error reporting from the schedd is not as good as error reporting from
   a command-line tool, but changing that is out of scope.
*  It ensures that the snapshot-in-time of the contents of the bucket occurs
   at a well-known and readily-comprehensible time.  It also allows the tool
   to immediately record provenance information, and/or inform the user of it.
*  The tool would also know how many objects were in the iterator.  It could
   let the user know as a quick sanity check ("I expected to submit 10 jobs
   and it said I submitted 10,000!"), and/or help implement policy in a
   user-friendly way ("You're trying to submit more than 100,000 jobs; are
   you sure you want to do that?").

Presuming the implementation is permitted to use Boto3 (now the official AWS
Python API, it seems), implementation should be simple.  We could, if desired, even not
attempt to load the Boto3 library unless necessary, minimizing packaging
dependencies (although Boto is available in RPMs as far back as CentOS 7.9).

We could also do a C++ implementation -- adding `ListObjectsV2()` to the existing ec2
gahp -- and call the resulting updated GAHP from Python.  The ec2 gahp already supports
uploading files to EC2, which suggests that most of the hard work is already done, but
this approach would still take considerably longer to implement.  The obvious gain would
be not requiring the Boto3 library.  Listing the contents of an S3 bucket from C++ code
may also be convenient for as-yet unpsecified future features.
