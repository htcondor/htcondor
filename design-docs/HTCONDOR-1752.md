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
iterator = bucket [var-name] bucket-url
```
where *var-name* is the optional name of the variable that will iterate over
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

The above blithely ignores that 2 (or sometimes 3) different security security
things could be needed to list the contents of an S3 bucket.  [FIXME]

### Implementation

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
