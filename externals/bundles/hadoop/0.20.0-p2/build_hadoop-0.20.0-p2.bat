rm -rf ..\..\install\hdfs

mkdir ..\..\install\hdfs
mkdir ..\..\install\hdfs\conf

cp -r %PACKAGE_NAME%\webapps ..\..\install\hdfs
cp -r %PACKAGE_NAME%\lib ..\..\install\hdfs
cp %PACKAGE_NAME%\hadoop-0.20.0-p1-core.jar ..\..\install\hdfs\lib
cp %PACKAGE_NAME%\src\hdfs\hdfs-default.xml ..\..\install\hdfs\conf