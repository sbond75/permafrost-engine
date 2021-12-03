{ lib, buildPythonPackage, fetchPypi }:

# https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/python.section.md
buildPythonPackage rec {
  pname = "six";
  version = "1.16.0";
  
  src =  fetchPypi {
    pname = "six";
    inherit version;
    sha256 = "09n9qih9rpj95q3r4a40li7hk6swma11syvgwdc68qm1fxsc6q8y";
  };

  doCheck = false;

  propagatedBuildInputs = [
  ];
  
  meta = with lib; {
    #homepage = "https://github.com/pytoolz/toolz";
    #description = "List processing tools and functional utilities";
    #license = licenses.bsd3;
    #maintainers = with maintainers; [ fridh ];
  };
}
