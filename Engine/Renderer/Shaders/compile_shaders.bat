@echo off
set SHADERC=..\..\..\build\bin\Debug\shaderc.exe
if not exist "%SHADERC%" (
    set SHADERC=..\..\..\build\_deps\bgfx-build\cmake\shaderc\Debug\shaderc.exe
)
if not exist "%SHADERC%" (
    set SHADERC=..\..\..\build\shaderc\Debug\shaderc.exe
)
if not exist "%SHADERC%" (
    echo shaderc.exe bulunamadi! BGFX_BUILD_TOOLS=ON ile build aldiginiza emin olun.
    exit /b 1
)

set INCLUDE_PATH=..\..\..\build\_deps\bgfx-src\bgfx\src

echo Compiling Vertex Shader...
"%SHADERC%" -f vs_pbr.sc -o vs_pbr.bin --type vertex --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f vs_skinned_pbr.sc -o vs_skinned_pbr.bin --type vertex --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\

echo Compiling Fragment Shader...
"%SHADERC%" -f fs_pbr.sc -o fs_pbr.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\

echo Done!
