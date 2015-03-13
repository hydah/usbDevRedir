call "D:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"

cd /d %~dp0

msbuild "usbDevRedir.sln" /p:Configuration=Release /m  /target:Rebuild