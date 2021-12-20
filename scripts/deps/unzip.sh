# https://stackoverflow.com/questions/36132350/install-python-wheel-file-without-using-pip

#echo *.whl | xargs -I{} -n 1 echo unzip {} -d "{}%.*"
