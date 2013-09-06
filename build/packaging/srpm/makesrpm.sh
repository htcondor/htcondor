#!/bin/bash
set -e
if [[ $VERBOSE ]]; then
  set -x
fi

# makesrpm.sh - generates rpms from the currently checked out HEAD,
# along with condor.spec and the sources in this directory

usage () {
  echo "usage: [VERBOSE=1] $(basename "$0") [options] [--] [rpmbuild-options]"
  echo "Options:"
  echo "  -ba    Build binary and source packages"
  echo "  -bs    Build source package only (default)"
  echo
  echo "  --bundle-std-univ-externals  Include std univ externals in src.rpm"
  echo "  --bundle-uw-externals        Include other UW externals in src.rpm"
  echo "  --bundle-all-externals       Include all externals in src.rpm"
  echo "  --externals-location {PATH|URL}  Use external sources from location"
  echo "                       (default=$externals_download)"
  echo
  echo "  --git-revision COMMIT   Use condor source from git tag or hash" \
                                                         "(default=HEAD)"
  echo "  --condor-release X.Y.Z  Use condor release tarball for version X.Y.Z"
  echo
  echo "Environment:"
  echo "  VERBOSE=1                         Show all commands run by script"
  echo "  BUNDLE_EXTERNALS_FROM={PATH|URL}  Provide default externals location"
  exit $1
}

fail () { echo "$@" >&2; exit 1; }

# defaults
buildmethod=-bs
externals_download=http://parrot.cs.wisc.edu/externals
externals_location=${BUNDLE_EXTERNALS_FROM:-$externals_download}
checkout_commit=HEAD

while [[ $1 = -* ]]; do
case $1 in
  -ba | -bs ) buildmethod=$1; shift ;;

  --bundle-uw-externals       ) uw_externals=1;              shift ;;
  --bundle-std-univ-externals ) std_univ_externals=1;        shift ;;
  --bundle-all-externals      ) uw_externals=1
                                std_univ_externals=1;        shift ;;
  --externals-location        ) externals_location=$2;       shift 2 ;;
  --externals-location=*      ) externals_location=${1#*=};  shift ;;

  --git-revision   ) checkout_commit=$2; shift 2 ;;
  --condor-release ) condor_release=$2; shift 2 ;;

  --help ) usage ;;
  -- ) shift; break ;;
# *  ) break ;;               # assume remaining args are rpmbuild-options
  *  ) usage 1 ;;             # assume remaining args are usage errors
esac
done

check_version_string () {
  [[ ${!1} =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Bad ${1//_/ }: '${!1}'"
}

if [[ $condor_release ]]; then
  if [[ $checkout_commit != HEAD ]]; then
    fail "Options --git-revision and --condor-release are mutually exclusive."
  fi
  check_version_string  condor_release
  checkout_commit=V${condor_release//./_}
fi

download_external () { wget -nv -O "$1" "$externals_location/$1"; }
copy_external     () { cp -p "$externals_location/$1" .;          }

if [[ $uw_externals || $std_univ_externals ]]; then
  case $externals_location in
    http://* | https://* | \
     ftp://* ) get_external=download_external ;;
       *://* ) fail "Unrecognized URL: '$externals_location'" ;;
          /* ) get_external=copy_external ;;
           * ) externals_location=$PWD/$externals_location
               get_external=copy_external ;;
  esac

  if [[ $externals_location = /* && ! -d $externals_location ]]; then
    fail "Path to externals location does not exist: '$externals_location'"
  fi
fi

# Do everything in a temp dir that will go away on errors or end of script
tmpd=$(mktemp -d "$PWD/.tmpXXXXXX")
trap 'rm -rf "$tmpd"' EXIT

WD=$PWD                                # save working dir
cd "$(dirname "$0")"                   # go to srpm source dir
srpm_dir=$PWD
cd "$(git rev-parse --show-toplevel)"  # go to root of git tree

git_rev=$(git rev-parse --short "$checkout_commit") \
|| fail "Couldn't find git rev for '$checkout_commit'"

condor_version=$(
  git show "$checkout_commit":CMakeLists.txt \
  | awk -F\" '/^set\(VERSION / {print $2}'
)

[[ $condor_version ]] || fail "Condor version string not found"
check_version_string  condor_version

git archive "$checkout_commit" | gzip > "$tmpd/condor.tar.gz"

cd "$tmpd"
mkdir SOURCES
cp -p "$srpm_dir"/* SOURCES
mv condor.tar.gz SOURCES

update_spec_define () {
  sed -i "s|^ *%define  *$1 .*|%define $1 $2|" SOURCES/condor.spec
}

update_spec_define git_rev "$git_rev"
update_spec_define tarball_version "$condor_version"

get_sources_from_file () {
  cd SOURCES
  for file in $(< "$srpm_dir/$1"); do
    $get_external "$file"
  done
  cd ..
}

if [[ $uw_externals ]]; then
  get_sources_from_file external-uw-sources
  update_spec_define    bundle_uw_externals 1
fi

if [[ $std_univ_externals ]]; then
  get_sources_from_file external-std-univ-sources
  update_spec_define    bundle_std_univ_externals 1
fi

rpmbuild $buildmethod "$@" -D"_topdir $tmpd" SOURCES/condor.spec

mv *RPMS/*.rpm "$WD"

