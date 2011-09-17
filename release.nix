# This is a nix deriviation for building all of the condor documentation
# For more info see http://nixos.org/

{ nixpkgs ? ../nixpkgs }:

{
    build =
        with import nixpkgs {};

        stdenv.mkDerivation {
            name = "condor-doc";
            version = "7.3.2";
            src = ./.;

            buildInputs = [ tetex transfig ghostscript netpbm latex2html ncompress ];
            phases = "unpackPhase buildPhase fixupPhase";
            builder = ./builder.sh;
        };
}
