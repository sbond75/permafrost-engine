{ lib, buildPythonPackage, fetchPypi, callPackage }:

# https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/python.section.md
buildPythonPackage rec {
  pname = "zeroconf-py2compat";
  version = "0.19.13";
  
  src =  fetchPypi {
    pname = "zeroconf-py2compat";
    inherit version;
    sha256 = "1q5lrrqd3d621j5mvwd4r7m8ijwip5cpvfqanmsff2gyaqqlwydz";
  };

  doCheck = false;

  propagatedBuildInputs = [
    (callPackage ./six.nix {})
    (callPackage ./ifaddr.nix {})
  ];
  
  meta = with lib; {
    #homepage = "https://github.com/pytoolz/toolz";
    #description = "List processing tools and functional utilities";
    #license = licenses.bsd3;
    #maintainers = with maintainers; [ fridh ];
  };
}
