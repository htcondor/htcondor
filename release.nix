{ nixpkgs ? ../nixpkgs }:

let condorVersion = "gitmaster"; in

let jobs = rec {
    tarball =
        { CONDOR_SRC ? { outPath = ./.; rev = "undef"; },
          manual ? (import ../CONDOR_DOC/release.nix {}).build,
          officialRelease ? false
        }:

        with import nixpkgs {};
        with { version = condorVersion; };

        releaseTools.sourceTarball {
            inherit version officialRelease;
            name = "condor-tarball";
            src = CONDOR_SRC;
            doDist = true;

            buildInputs = [ perl which bison ];

            preAutoconf = ''
                pushd src
            '';
            postAutoconf = ''
                popd
            '';

            # condor has no "make dist", remove a few files and make one manually
            distPhase = ''
                ensureDir $out/tarballs
                mkdir condor-${version}
                cp -a src config externals imake LICENSE-2.0.txt msconfig NOTICE.txt condor-${version}
                mkdir -p condor-${version}/externals/bundles/man/current
                ( cd ${manual} && tar cvhf - man/ ) | gzip >condor-${version}/externals/bundles/man/current/man-current.tar.gz
                echo $rev > condor-${version}/git-revision
                tar cvf - condor-${version} | gzip >$out/tarballs/condor-${version}pre_${CONDOR_SRC.rev}.tar.gz
            '';

        };

    build =
        { tarball ? jobs.tarball {}
        , system ? "i686-linux"
        }:
        with import nixpkgs {};
        with { version = condorVersion; };

        releaseTools.nixBuild {
            name = "condor";
            src = tarball;

            buildInputs = [ perl which bison classads pcre xlibs.imake flex postgresql xlibs.libX11 xlibs.xproto gsoap libvirt ];

            patchPhase = ''
                echo 'imake -s Makefile -I. -I.. -I../.. -I../config -I../../config $*' >src/condor_imake
                grep -R -H '\/usr\/bin\/env' . | awk -F: '{print $1}' | (while read f; do substituteInPlace $f --replace /usr/bin/env $(type -tp env); done)
            '';

            preConfigure = "cd src";

            configureFlags = "--with-glibc=${glibc} --disable-glibc-version-check --disable-gcc-version-check --enable-proper --disable-full-port --with-postgresql=${postgresql}";

            installTargets = "strip.tar";
            postInstall = ''
                ( cd $out && tar xvf - ) < strip_dir/release.tar
            '';

            doCheck = false;
        };
};

in jobs
