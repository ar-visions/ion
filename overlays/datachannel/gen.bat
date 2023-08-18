@echo on

rem cmake -G "NMake Makefiles" -DUSE_GNUTLS=1 -DGNUTLS_INCLUDE_DIR='%PREFIX%/include/gnutls'
rem    -DNettle_INCLUDE_DIR='%PREFIX%/include/nettle' -DNettle_LIBRARY='libnettle'

cmake.exe -S . -B ion-build -DCI='C:\Users\kalen\src\prefix/ci' -DCMAKE_INSTALL_PREFIX='C:\Users\kalen\src\prefix\install' -DCMAKE_INSTALL_DATAROOTDIR='C:\Users\kalen\src\prefix/install/share' -DCMAKE_INSTALL_DOCDIR='C:\Users\kalen\src\prefix/install/doc' -DCMAKE_INSTALL_INCLUDEDIR='C:\Users\kalen\src\prefix/install/include' -DCMAKE_INSTALL_LIBDIR='C:\Users\kalen\src\prefix/install/lib' -DCMAKE_INSTALL_BINDIR='C:\Users\kalen\src\prefix/install/bin' -DCMAKE_PREFIX_PATH='C:\Users\kalen\src\prefix/install' -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_BUILD_TYPE=Debug -G "NMake Makefiles" -DUSE_GNUTLS=1 -DGNUTLS_LIBRARY=gnutls -DGNUTLS_INCLUDE_DIR=C:\Users\kalen\src\prefix/install/include/gnutls -DNettle_INCLUDE_DIR="C:\Users\kalen\src\prefix\install\include\nettle" -DNettle_LIBRARY=libnettle
