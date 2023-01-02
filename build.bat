@echo off

SET INCLUDES=-Iext\
SET SRC=src\main.cpp
SET WARNING_FLAGS=-W4 -WX -wd4100 -wd4101 -wd4189 -wd4996 -wd4530
SET COMPILER_FLAGS=-nologo -FC -MD -Zi %WARNING_FLAGS% -Febin\Graphics.exe -Fobin\ -Fdbin\

CL %COMPILER_FLAGS% %INCLUDES% %SRC%
