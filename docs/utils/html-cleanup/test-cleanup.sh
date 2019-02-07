#!/bin/sh

./html-cleanup.py UsingCondorannexfortheFirstTime.html
pandoc -f html -t rst UsingCondorannexfortheFirstTime.html.out -o ~/code/readthedocs/docs/testing/using-annex-first-time.rst
./html-cleanup.py SubmittingaJob.html
pandoc -f html -t rst SubmittingaJob.html.out -o ~/code/readthedocs/docs/testing/submitting-a-job.rst
./html-cleanup.py JobClassAdAttributes.html
pandoc -f html -t rst JobClassAdAttributes.html.out -o ~/code/readthedocs/docs/testing/job-classad-attributes.rst
