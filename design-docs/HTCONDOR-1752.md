## Allow `htcondor jobsubset submit` to iterate over S3 objects

Presently, one may write `queue FILE matching files *.dat` as a line of a
submit file and submit a new job for each file (in the IWD) matching the
glob `*.dat`, with the specific name being available in `$(FILE)` for
substitution.  What would it take to make this work -- only from
`htcondor jobset`? -- with an S3 URL and the corresponding list of objects?

### Specifications

The `htcondor jobset` tool implements the `iterator` keyword, which specifies,
like the `queue` statement, how HTCondor should create multiple jobs from the
same specification.  (We'll skip, for now, all the reason why we should remove
the `iterator` keyword and just allow the `queue` syntax to create jobsets.)
The `iterator` keyword already requires an *iterator-type* argument, so it
will be fairly trivial to add a `bucket` type, and until we unify the two syntaxes,
there's no real reason to implement `queue matching buckets` instead.

The line in the jobset file would then look like the following:
```
iterator = bucket var-name bucket-url token-file token-file [token-file]
```
where *var-name* is the name of the variable that will iterate over
the objects in the bucket, and *bucket-url* is the URL of the bucket in question.
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

Although the natural (orthogonal) approach to specifying security tokens
would additional attributes specifying those security tokens in the jobset
file, to minimize the scope
of change, we do not propose a general mechanism by which the user can
specify arbitrary key-value pairs to propogate into each submit file; instead,
the `bucket` iterator type will accept multiple arguments: the
*bucket-url*, the name of the file containing the access key, and
the name of the file containing the secret key, and (optionally), the
name of the file containing the session token, in that order.  This retains
the same general ordering as the `table` iterator type, where the variable
names precede the file name or table data.  As a special case, the *bucket*
iterator will set the S3 credential attributes to the values in its arguments
for each job in the set.

### Implementation

Given that the current iterator is implemented client-side, we propose a
client-side iterator for S3 buckets, as well.  A client-side iterator has
a number of advantages:

*  Early error checking and better error-reporting.  It's a little sad that
   error reporting from the schedd is not as good as error reporting from
   a command-line tool, but changing that is out of scope.
*  It ensures that the snapshot-in-time of the contents of the bucket occurs
   at a well-known and readily-comprehensible time.  It also allows the tool
   to immediately record provenance information, and/or inform the user.
*  The tool would also know how many objects were in the iterator.  It could
   let the user know as a quick sanity check ("I expected to submit 10 jobs
   and it said I submitted 10,000!"), and/or help implement policy in a
   user-friendly way ("You're trying to submit more than 100,000 jobs; are
   you sure you want to do that?").

Presuming the implementation is permitted to use Boto3 (now AWS' official Python
API, AFAICT), implementation should be simple.  We could, if desired, even not
attempt to load the Boto3 library unless necessary, minimizing packaging
dependencies (although, FWIW, Boto is available in RPMs as far back as CentOS 7.9).
A pure-Python implementation like this would probably not be hard to make available
as a script that `condor_submit` could call to handle `queue matching objects`,
were that to become a feature, and we would probably want to make that an external
process anyway (for all the reasons that the ec2 gahp currently is).

We could also do a C++ implementation -- adding `ListObjectsV2()` to the existing ec2
gahp -- and call the resulting updated GAHP from Python.  The ec2 gahp already supports
uploading files to EC2, which suggests that most of the hard work is already done, but
this approach would still take considerably longer to implement.  The obvious gain would
be not requiring the Boto3 library.  Listing the contents of an S3 bucket from C++ code
may also be convenient for as-yet unpsecified future features.
