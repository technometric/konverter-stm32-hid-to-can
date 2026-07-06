@echo off
setlocal
set CC=g++
set MODE=-DNONE
REM set MODE=-DHID_MODE
REM set MODE=-DCDC_MODE

if not exist build mkdir build

echo === Compile (32-bit, XP-compatible) ===
%CC% -m32 -c src\download.cpp -o build\download_x86.o -Os -s -Wall -Wextra -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 %MODE% -Iinclude
%CC% -m32 -c src\upload.cpp -o build\upload_x86.o -Os -s -Wall -Wextra -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 %MODE% -Iinclude
%CC% -m32 -c src\serialcom.cpp -o build\serialcom_x86.o -Os -s -Wall -Wextra -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 %MODE% -Iinclude
%CC% -m32 -c src\cbs32.cpp -o build\cbs32_x86.o -Os -s -Wall -Wextra -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 %MODE% -Iinclude
%CC% -m32 -c src\stm32hid.cpp -o build\stm32hid_x86.o -Os -s -Wall -Wextra -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 %MODE% -Iinclude

echo === Linking DLL ===
%CC% -m32 -shared -o build\cbs32.dll build\cbs32_x86.o build\serialcom_x86.o build\download_x86.o build\upload_x86.o build\stm32hid_x86.o^
    -Wl,--kill-at -Wl,--add-stdcall-alias ^
    -static-libgcc -static-libstdc++ -s ^
    -lsetupapi -lole32 -lshlwapi -lhid

echo.
echo Built build\cbs32.dll (x86, XP-compatible)
endlocal
