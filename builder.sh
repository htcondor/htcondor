# This is the nix builder script for release.nix

source $stdenv/setup

buildPhase() {
    cd doc
    ensureDir $out

    ## html
    make TARGETDIR=$out/html ref.html

    ## nroff
    make just-man-pages
    cp -a man $out/man

    ## dvi
    make ref.dvi
    cp ref.dvi $out/manual.dvi

    ## ps
    make ref.ps
    cp ref.ps $out/manual.ps

    ## pdf
    make ref.pdf
    cp ref.pdf $out/manual.pdf

    # support hydra builds as published on http://hydra.batlab.org/project/condor/
    ensureDir $out/nix-support
    echo "doc manual $out/html" >>$out/nix-support/hydra-build-products
    echo "doc-pdf manual $out/manual.pdf" >> $out/nix-support/hydra-build-products
    echo "doc dvi $out/manual.dvi" >> $out/nix-support/hydra-build-products
    echo "doc ps $out/manual.ps" >> $out/nix-support/hydra-build-products
    echo "doc release-notes $out/html/8_Version_History.html" >> $out/nix-support/hydra-build-products
}

genericBuild
