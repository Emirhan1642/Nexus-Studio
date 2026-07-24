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
"%SHADERC%" -f vs_shadow.sc -o vs_shadow.bin --type vertex --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f vs_skinned_shadow.sc -o vs_skinned_shadow.bin --type vertex --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f vs_fullscreen.sc -o vs_fullscreen.bin --type vertex --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f vs_voxelize.sc -o vs_voxelize.bin --type vertex --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\


echo Compiling Fragment Shader...
"%SHADERC%" -f fs_pbr.sc -o fs_pbr.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f fs_shadow.sc -o fs_shadow.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f fs_bloom_threshold.sc -o fs_bloom_threshold.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f fs_bloom_blur.sc -o fs_bloom_blur.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f fs_tonemap.sc -o fs_tonemap.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f fs_ssgi.sc -o fs_ssgi.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f fs_voxelize.sc -o fs_voxelize.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\
"%SHADERC%" -f fs_fxaa.sc -o fs_fxaa.bin --type fragment --platform windows -p s_5_0 -i "%INCLUDE_PATH%" -i .\

echo Done!
