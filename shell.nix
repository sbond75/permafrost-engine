# Tip: direnv to keep dependencies for a specific project in Nix
# Run: nix-shell

{ useGtk ? true,
  pkgs ? import (builtins.fetchTarball { # https://nixos.wiki/wiki/FAQ/Pinning_Nixpkgs :
  # Descriptive name to make the store path easier to identify
  name = "nixos-unstable-2020-09-03";
  # Commit hash for nixos-unstable as of the date above
  url = "https://github.com/NixOS/nixpkgs/archive/702d1834218e483ab630a3414a47f3a537f94182.tar.gz";
  # Hash obtained using `nix-prefetch-url --unpack <url>`
  sha256 = "1vs08avqidij5mznb475k5qb74dkjvnsd745aix27qcw55rm0pwb";
}) { }}:
with pkgs;

let
  # https://stackoverflow.com/questions/42136197/how-to-override-compile-flags-for-a-single-package-in-nixos
  optimizeWithFlags = pkg: flags:
    pkgs.lib.overrideDerivation pkg (old:
    let
      newflags = pkgs.lib.foldl' (acc: x: "${acc} ${x}") "" flags;
      oldflags = if (pkgs.lib.hasAttr "NIX_CFLAGS_COMPILE" old)
        then "${old.NIX_CFLAGS_COMPILE}"
        else "";
    in
    {
      NIX_CFLAGS_COMPILE = "${oldflags} ${newflags}";
    });
  optimizeForThisHost = pkg:
    optimizeWithFlags pkg [ "-O3" "-march=native" "-fPIC" ];
  withDebuggingCompiled = pkg: pkg;
#    optimizeWithFlags (pkg.overrideAttrs (old: rec { separateDebugInfo = true; dontStrip = true;  preConfigure = #old.preConfigure ++
 #                                                      ''cmakeFlags="$cmakeFlags -DCMAKE_BUILD_TYPE=Debug"''; })) [ "-DDEBUG" "-O0" "-g3" ];
  frameworks = pkgs.darwin.apple_sdk.frameworks;
  # https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/python.section.md
  # python-with-my-packages2 = let
  #   packageOverrides = self: super: {
  #     zeroconf-py2compat = {
  #       version = "0.19.13";
  #       src =  super.fetchPypi {
  #         pname = "zeroconf-py2compat";
  #         inherit version;
  #         sha256 = "08blshqj9zj1wyjhhw3kl2vas75vhhicvv72flvf1z3jvapgw295";
  #       };
  #     };
  #   };
  # in pkgs.python27.override {inherit packageOverrides; self = python-with-my-packages2;};
  python-with-my-packages = python27.withPackages (p: with p; [
    #pip # Now you can use something like `python -m pip download zeroconf-py2compat` to download pip packages to your shell's current directory
    (callPackage ./zeroconf-py2compat.nix {})
    (callPackage ./netifaces.nix {})
  ]);
in
mkShell {
  buildInputs = [
    (lib.optional (stdenv.hostPlatform.isMacOS) frameworks.Carbon)
    #] ++ (lib.optional (stdenv.hostPlatform.isLinux) [pulseaudio]) ++ [
    ] ++ (lib.optional (stdenv.hostPlatform.isLinux) [gdb]) ++ [
    (withDebuggingCompiled libGL)
    (withDebuggingCompiled glew)
    (withDebuggingCompiled openal)
    (withDebuggingCompiled SDL2)
    pkg-config

    python-with-my-packages
    #python-with-my-packages2.withPackages(ps: [ps.zeroconf-py2compat]).env
  ]; # Note: for macos need this: write this into the path indicated:
  # b) For `nix-env`, `nix-build`, `nix-shell` or any other Nix command you can add
  #   { allowUnsupportedSystem = true; }
  # to ~/.config/nixpkgs/config.nix.
  # ^^^^^^^^^^^^^^^^^^ This doesn't work, use `brew install cartr/qt4/pyqt` instead.
  
}
