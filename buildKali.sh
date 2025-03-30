cmake --preset kali-release
cmake --build build-kali-release
cd ./build-kali-release
ctest
cd ..
