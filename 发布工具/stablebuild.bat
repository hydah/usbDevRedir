@echo off
echo 编译usb 重定向及隔离项目......
echo ...
call "..\build.bat"

cd /d %~dp0
cd udr-stable
set dirname=udr-stable-%date:~0,4%%date:~5,2%%date:~8,2%
md "%dirname%"
cd "%dirname%"
md "udr-windows"
md "udr-linux"
set filename=%date:~0,4%%date:~5,2%%date:~8,2%-%time:~0,2%%time:~3,2%-setup.exe
echo "%filename%"

copy "..\..\..\udrInstall\安装包制作\Express\SingleImage\DiskImages\DISK1\setup.exe" "udr-windows\%filename%"
copy "..\..\linux-udr\udr-linux.tar" "udr-linux\."
copy "..\..\Readme.txt" .
echo ...
echo 完成！
pause