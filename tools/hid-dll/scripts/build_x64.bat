setlocal
set CC=g++
set MODE=-DNONE
REM set MODE=-DHID_MODE
REM set MODE=-DCDC_MODE

if not exist build mkdir build

echo === Compile (64-bit) ===
%CC% -m64 -c src\download.cpp -o build\download_x64.o -Os -s -Wall -Wextra %MODE% -Iinclude
%CC% -m64 -c src\upload.cpp -o build\upload_x64.o -Os -s -Wall -Wextra %MODE% -Iinclude
%CC% -m64 -c src\serialcom.cpp -o build\serialcom_x64.o -Os -s -Wall -Wextra %MODE% -Iinclude
%CC% -m64 -c src\cbs32.cpp -o build\cbs64_x64.o -Os -s -Wall -Wextra %MODE% -Iinclude

echo === Linking DLL ===
%CC% -m64 -shared -o build\cbs64.dll build\cbs64_x64.o build\serialcom_x64.o build\download_x64.o build\upload_x64.o^
    -Wl,--kill-at -Wl,--add-stdcall-alias ^
    -static-libgcc -static-libstdc++ -s ^
    -lsetupapi -lole32 -lshlwapi -lhid

echo.
echo Built build\cbs64.dll (x64)
endlocal