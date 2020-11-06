@echo off

if "--%1%--" == "--p--" goto make-p
goto unknown

:make-p
set output=push
set c-files=push.c
echo gcc -Ofast -o %output% %c-files%
gcc -Ofast -o %output% %c-files%
if "--%2%--" == "--1--" push push.txt
goto done


:unknown
echo Unknown make. I know how to make these:
echo.
echo    p  - makes push.exe
echo.
echo    NOTE: if arg2 is given it then runs the program

:done
