@echo off

del cmakecache.txt
copy win\vs8cache.txt cmakecache.txt
cmake -G "Visual Studio 8 2005"
copy cmakecache.txt win\vs8cache.txt
