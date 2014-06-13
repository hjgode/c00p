@echo off

ECHO ############## START ###############
echo  MAKE SHURE THE RADIOS ARE OFF TO 
echo  AVOID AUTOMATIC TIME SYNCS!
echo .
PAUSE Press any key to start

REM copy actual debug binary to test dir
echo "Copy of actual binary..."
copy /Y "..\Windows Mobile 6 Professional SDK (ARMV4I)\Debug\iTimedReboot2.exe" . >NUL
echo "...done"

rem Copy files...
echo "Copy tools..."
pdel \iTimedReboot2.exe.log.txt >NUL
pput -f ./iTimedReboot2.exe \iTimedReboot2.exe
pdel \setDateTime.exe.log.txt >NUL
pput -f ./setDateTime.exe \setDateTime.exe
echo "...done"

REM clear local test log file
ECHO ############## START ############### >test_run.txt

REM TEST1
call:mylog "======================================================"
call:mylog " TEST1..."
call:mylog "======================================================"
rem setze system datum auf 4.6.2014
rem setze system zeit auf 14:00
SET systime=201406041400
call:mysetdatetime %systime%
rem setze daysinterval auf 1
set mydayinterval=1
rem setze lastrebootdate auf 4.6.2014, next reboot waere dann am 5.6.2016 2:00
set mylastreboot=20140604
rem setze reboottime auf 02:00 
set myreboottime=02:00
call:mySetRebootDateTimeAndInterval %mylastreboot%, "%myreboottime%", %mydayinterval%
rem now test this
call:MYDOTEST 1

REM TEST2
call:mylog "======================================================"
call:mylog " TEST2..."
call:mylog "    set time 2 minutes before reboot"
call:mylog "======================================================"
SET systime=201406060234
call:mysetdatetime %systime%
rem setze daysinterval auf 1
set mydayinterval=1
set mylastreboot=20140605
set myreboottime=02:36
call:mySetRebootDateTimeAndInterval %mylastreboot%, "%myreboottime%", %mydayinterval%
rem now test this
call:MYDOTEST 2

REM TEST3
call:mylog "======================================================"
call:mylog " TEST3..."
call:mylog "    test systemzeit zu rebootzeit"
call:mylog "======================================================"
SET systime=201406050236
call:mysetdatetime %systime%
rem setze daysinterval auf 1
set mydayinterval=1
set mylastreboot=20140604
rem setze reboottime auf 15:00 
set myreboottime=02:36
call:mySetRebootDateTimeAndInterval %mylastreboot%, "%myreboottime%", %mydayinterval%
rem now test this
call:MYDOTEST 3

REM TEST4
call:mylog "======================================================"
call:mylog " TEST4..."
call:mylog "   test systemzeit zu 10 minuten nach rebootzeit"
call:mylog "======================================================"
SET systime=201406050246
call:mysetdatetime %systime%
rem setze daysinterval auf 1
set mydayinterval=1
set mylastreboot=20140604
set myreboottime=02:36
call:mySetRebootDateTimeAndInterval %mylastreboot%, "%myreboottime%", %mydayinterval%
rem now test this
call:MYDOTEST 4
        
REM TEST5
call:mylog "======================================================"
call:mylog " TEST5..."
call:mylog "   test systemzeit weit nach letzter rebootzeit"
call:mylog "======================================================"
SET systime=201406051500
call:mysetdatetime %systime%
rem setze daysinterval auf 7
set mydayinterval=1
set mylastreboot=20140506
rem setze reboottime auf 15:00 
set myreboottime=15:00
call:mySetRebootDateTimeAndInterval %mylastreboot%, "%myreboottime%", %mydayinterval%
rem now test this
call:MYDOTEST 5

REM ###############################################################
GOTO END_TESTS
REM ###############################################################
                 
REM TEST5
call:mylog "======================================================"
call:mylog " TEST6..."
call:mylog "   test systemzeit weit vor letzter rebootzeit"
call:mylog "======================================================"
rem setze system datum auf 11.11.2014
rem setze system zeit auf 15:00
SET systime=201411111500
call:mysetdatetime %systime%
rem setze daysinterval auf 7
set mydayinterval=7
REM 2. april 2006 was a sunday
rem setze lastrebootdate auf 2.4.2006, next reboot waere dann am Sonntag den 16.11.2014 15:00
Rem lastreboot sollte dann der 9.11.2014 sein
set mylastreboot=20060402
rem setze reboottime auf 15:00 
set myreboottime=15:00
call:mySetRebootDateTimeAndInterval %mylastreboot%, "%myreboottime%", %mydayinterval%
rem now test this
call:MYDOTEST 6
                                     
goto:weiter
rem some date time functions and calculations
set mydate=%date:~6,4%%date:~3,2%%date:~0,2%
REM date is 05.12.2013
echo date is %date%
echo datetime is %mydate%
set day=%date:~0,2%
REM time is 10:51:03,27
echo time is %time%
set mytime=%time:~0,2%%time:~3,2%
set /a newtime=%time:~3,2%-5
echo time is %mytime%, newtime is %newtime%
set newtime=%mytime:~0,2%:%newtime%
echo newtime is now %newtime%
:weiter

pause

goto:END_TESTS

:MYDOTEST
REM call with call:MYDOTEST 1
REM start iTimedReboot2
prun \iTimedReboot2.exe -test
call:mylog "Result:"
REM show reg entries
pregutl HKLM\Software\Intermec\iTimedReboot2
REM read back registry into log file
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
call:mylog "*************** LOG  TEST%~1 ****************"
REM get iTimedReboot2 log file
pget -f \iTimedReboot2.exe.log.txt
REM cat logfile to test log
type iTimedReboot2.exe.log.txt >>test_run.txt
REM show log
type iTimedReboot2.exe.log.txt
REM delete logfile on device
pdel \iTimedReboot2.exe.log.txt
REM write marker to test log
call:mylog "---------------TEST%~1  ---------------"
ECHO .>>test_run.txt
goto:eof

:END_TESTS
REM this will filter some strings and show matching lines on screen
REM findstr /g:find_start.txt iTimedReboot2.exe.log.txt
GOTO MYEND

:MYWAIT
echo Sleeping 1 sec ...
PING 1.1.1.1 -n 1 -w 1000 >NUL
echo ...continue
GOTO :eof

:MYWAIT3
echo Sleeping 3 sec ...
PING 1.1.1.1 -n 1 -w 3000 >NUL
echo ...continue
GOTO :eof

:mySetRebootDateTimeAndInterval
rem call with 'call:mySetRebootDateTime 20130512,"15:00",2'
rem to set date to 12. may 2013  and reboot time to 15:00 and day interval 2
echo.
Rem *************************************************************************
echo ...patch the reg file
Rem *************************************************************************
REM replace all !LASTBOOT! with new date
sed "s/!LASTBOOT!/%~1/ig;s/!REBOOTTIME!/%~2/ig;s/!REBOOTDAYS!/%~3/ig" iTimedReboot2.reg.tmp >iTimedReboot2.reg
rem sed "s/!REBOOTTIME!/%~2/ig" iTimedReboot2.reg.tmp >iTimedReboot2.reg
rem sed "s/!REBOOTDAYS!/%~3/ig" iTimedReboot2.reg.tmp >iTimedReboot2.reg
call:mylog "Importing test reg keys..."
pregutl @iTimedReboot2.reg >>test_run.txt
rem to return a value use a var: 
rem set "var1=DosTips"
set dt=%~1
set tt=%~2
set dt1=%tt% %dt:~6,2%.%dt:~4,2%.%dt:~0,4%
call:mylog "reboot settings now lastreboot=%dt1%, days interval=%~3"
goto:eof

:mysetdatetime
rem call with call:mysetdatetime 201311101400
prun \SetDateTime.exe %~1
REM call:mylog "system time now %~1"
REM show as 14:00 11.10.2013
set dt=%~1
set dt1=%dt:~8,2%:%dt:~10,2% %dt:~6,2%.%dt:~4,2%.%dt:~0,4%
call:mylog "system time now %dt1%"
goto:eof

:mylog
rem call with mylog "text to show and log"
echo %~1
echo %~1 >>test_run.txt
goto:eof

:MYEND
@echo on
