{ lib, buildPythonPackage, fetchPypi }:

# https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/python.section.md
buildPythonPackage rec {
  pname = "ipaddress";
  version = "1.0.23";
  
  src =  fetchPypi {
    pname = "ipaddress";
    inherit version;
    sha256 = "1qp743h30s04m3cg3yk3fycad930jv17q7dsslj4mfw0jlvf1y5p";
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
