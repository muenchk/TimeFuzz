cmake --preset kali
cmake --build buildKali --config Release
cd ./buildKali
ctest
cd ..
