# codegen and build driver until cmake integration
#!/bin/sh
export WSFCPP_HOME=/usr

# generate our cpp types from WSDL
rm -fr codegen/ include/ lib/
WSDL2CPP.sh -uri etc/aviary-job.wsdl -d adb -ss -g -o codegen/job
WSDL2CPP.sh -uri etc/aviary-query.wsdl -d adb -ss -g -o codegen/query

# stow the headers for others steps in the build
if ! test -d include; then
    mkdir include;
fi
cp codegen/job/*.h codegen/job/src/*.h include;
cp codegen/query/*.h codegen/query/src/*.h include;
rm -f include/Aviary*.h

# get rid of the extraneous stuff that WSDL2CPP won't let us turn off
rm -f codegen/job/*.{h,cpp,vcproj}

# setup our lib dir
if ! test -d lib; then
    mkdir lib;
fi

# build the job WSDL/XSD cpp types
g++ -g -shared -o lib/libaviary_job_types.so \
        -I. \
        -Iinclude \
        -I$WSFCPP_HOME/include \
        -I$WSFCPP_HOME/include/axis2-1.6.0 \
        -I$WSFCPP_HOME/include/axis2-1.6.0/platforms \
        -L$WSFCPP_HOME/lib \
        -laxutil \
        -laxis2_axiom \
        -laxis2_engine \
        -laxis2_parser \
        -lpthread \
        -laxis2_http_sender \
        -laxis2_http_receiver \
        -lguththila \
        -lwso2_wsf \
         codegen/job/src/*.cpp

# build the job WSDL/XSD cpp types
g++ -g -shared -o lib/libaviary_query_types.so \
        -I. \
        -Iinclude \
        -I$WSFCPP_HOME/include \
        -I$WSFCPP_HOME/include/axis2-1.6.0 \
        -I$WSFCPP_HOME/include/axis2-1.6.0/platforms \
        -L$WSFCPP_HOME/lib \
        -laxutil \
        -laxis2_axiom \
        -laxis2_engine \
        -laxis2_parser \
        -lpthread \
        -laxis2_http_sender \
        -laxis2_http_receiver \
        -lguththila \
        -lwso2_wsf \
         codegen/query/src/*.cpp
