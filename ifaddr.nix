{ lib, buildPythonPackage, fetchPypi, callPackage }:

# https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/python.section.md
buildPythonPackage rec {
  pname = "ifaddr";
  version = "0.1.7";
  
  src =  fetchPypi {
    pname = "ifaddr";
    inherit version;
    sha256 = "150sxdlicwrphmhnv03ykxplyd2jdrxz0mikgnivavgilrn8m7hz";
  };

  doCheck = false;

  propagatedBuildInputs = [
    (callPackage ./ipaddress.nix {})
  ];
  
  meta = with lib; {
    #homepage = "https://github.com/pytoolz/toolz";
    #description = "List processing tools and functional utilities";
    #license = licenses.bsd3;
    #maintainers = with maintainers; [ fridh ];
  };
}
