rm -rf ..\..\install\hdfs

mkdir ..\..\install\hdfs
mkdir ..\..\install\hdfs\conf

cp -r %PACKAGE_SRC_NAME%\webapps ..\..\install\hdfs
cp -r %PACKAGE_SRC_NAME%\lib ..\..\install\hdfs
cp %PACKAGE_SRC_NAME%\hadoop-common-0.21.0.jar ..\..\install\hdfs\
cp %PACKAGE_SRC_NAME%\hadoop-hdfs-0.21.0.jar ..\..\install\hdfs
cp %PACKAGE_SRC_NAME%\hdfs\src\java\hdfs-default.xml ..\..\install\hdfs\conf
