#BUILD
mkdir build
cd build

cmake ..
cmake --build .

#Run
sqlite_clone.exe

The default database is stored at `data/litedb.db`. Generated database files, catalogs, WAL files, and indexes belong under `data/` and are ignored by Git. All CMake build output belongs under `build/`.

