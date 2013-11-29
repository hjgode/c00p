@echo off

ECHO ############## START ###############
echo  MAKE SHURE THE RADIOS ARE OFF TO 
echo  AVOID AUTOMATIC TIME SYNCS!
echo .
PAUSE Press any key to start

copy /Y "..\Windows Mobile 6 Professional SDK (ARMV4I)\Debug\iTimedReboot2.exe" .

ECHO ############## START ############### >test_run.txt

echo Importing test reg keys...
echo Importing test reg keys... >>test_run.txt
pregutl @iTimedReboot2_one_day.reg >>test_run.txt

REM the reg should now contain
rem LastBootDate=20131129
rem RebootDays=0
rem RebootTime=12:45

rem Copy files...
pdel \iTimedReboot2.exe.log.txt >NUL
pput -f ./iTimedReboot2.exe \iTimedReboot2.exe
pdel \setDateTime.exe.log.txt >NUL
pput -f ./setDateTime.exe \setDateTime.exe

ECHO TEST1...
ECHO 	set time manually to 29.11.2013 11:00
echo 	run iTimedReboot2.exe
ECHO ++++++++++++++ TEST1 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
ECHO set time manually to 01.01.2013 11:00 >>test_run.txt
echo    simulate a iTimedReboot2 call before valid date >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201311291100
CALL :MYWAIT
prun \iTimedReboot2.exe -test
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG  TEST1 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST1  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST2...
ECHO 	set time manually to 01.01.2013 13:00
echo    simulate a iTimedReboot2 call after last reboot time
echo 	run iTimedReboot2.exe
ECHO ++++++++++++++ TEST2 ++++++++++++++++ >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2  >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
ECHO set time manually to 01.01.2013 13:00 >>test_run.txt
echo    simulate a iTimedReboot2 call after reboot >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201311291300
CALL :MYWAIT
prun \iTimedReboot2.exe -test
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG TEST2**************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST2  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST3...
ECHO 	set time manually to 29.11.2013 12:45
ECHO 	run iTimedReboot2.exe 
ECHO ++++++++++++++ TEST3 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
ECHO set time manually to 01.01.2013 12:00, the time to reboot>>test_run.txt
ECHO 	iTimedReboot2.exe >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201311291245
CALL :MYWAIT
prun \iTimedReboot2.exe -test
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 3 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST3  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST4...
ECHO 	set time manually to 29.11.2013 12:47
ECHO 	iTimedReboot2.exe 
ECHO ++++++++++++++ TEST4 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
ECHO set time manually to 29.11.2013 12:47, within reboot time >>test_run.txt
ECHO 	iTimedReboot2.exe    >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201311291247
CALL :MYWAIT
prun \iTimedReboot2.exe -test 
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG  test 4 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST4  --------------- >>test_run.txt
ECHO .>>test_run.txt

GOTO END_TESTS

ECHO TEST5...
echo 	simulates a scheduler call at correct time
echo 	set time manually to 02.12.2011 19:55
echo 	iTimedReboot2.exe -s task2
ECHO ++++++++++++++ TEST5 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
echo ###	simulates a scheduler call at correct time  >>test_run.txt
echo set time manually to 02.12.2011 19:55  >>test_run.txt
echo 	iTimedReboot2.exe -s task2  >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201112021955
CALL :MYWAIT
prun \iTimedReboot2.exe -test -s task2
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 5 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST5  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST6...
echo 	simulates a scheduler call at correct time
echo 	set time manually to 02.12.2011 20:00
echo 	iTimedReboot2.exe -s task3
ECHO ++++++++++++++ TEST6 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
echo ###	simulates a scheduler call at correct time  >>test_run.txt
echo set time manually to 02.12.2011 20:00  >>test_run.txt
echo 	iTimedReboot2.exe -s task3  >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201112022000
CALL :MYWAIT
prun \iTimedReboot2.exe -test -s task3
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 6 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST6  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST7...
echo 	simululates a scheduler call at correct time
echo 	set time manually to 03.12.2011 0600
echo 	iTimedReboot2.exe -k task1
ECHO ++++++++++++++ TEST7 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
echo ###	simulates a scheduler call at correct time  >>test_run.txt
echo set time manually to 03.12.2011 0600  >>test_run.txt
echo 	iTimedReboot2.exe -k task1  >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201112030600
CALL :MYWAIT
prun \iTimedReboot2.exe -test -k task1
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 7 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST7  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST8...
echo 	simululates a scheduler call at correct time
echo 	set time manually to 03.12.2011 0605
echo 	iTimedReboot2.exe -k task2

ECHO ++++++++++++++ TEST8 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
echo ###	simulates a scheduler call at correct time  >>test_run.txt
echo set time manually to 03.12.2011 0605  >>test_run.txt
echo 	iTimedReboot2.exe -k task2  >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201112030605
CALL :MYWAIT
prun \iTimedReboot2.exe -test -k task2
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 8 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST8  --------------- >>test_run.txt
ECHO .>>test_run.txt


ECHO TEST9...
echo 	simululates a delayed call
echo 	set time manually to 04.12.2011 1500
ECHO 	iTimedReboot2.exe -s task2  				
ECHO 	iTimedReboot2.exe -s task1  				
ECHO 	iTimedReboot2.exe -k task1  				
ECHO 	iTimedReboot2.exe -k task2  				
echo 	iTimedReboot2.exe   
ECHO ++++++++++++++ TEST9 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
echo ###	simululates a delayed call  >>test_run.txt
echo set time manually to 04.12.2011 1500  >>test_run.txt
ECHO 	iTimedReboot2.exe -s task2  				>>test_run.txt
ECHO 	iTimedReboot2.exe -s task1  				>>test_run.txt
ECHO 	iTimedReboot2.exe -k task1  				>>test_run.txt
ECHO 	iTimedReboot2.exe -k task2  				>>test_run.txt
echo 	iTimedReboot2.exe   >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201112041500
CALL :MYWAIT
prun \iTimedReboot2.exe -test -s task2
prun \iTimedReboot2.exe -test -s task1
prun \iTimedReboot2.exe -test -k task1
prun \iTimedReboot2.exe -test -k task2
prun \iTimedReboot2.exe -test 
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 9 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST9  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST10...
echo 	simululates a premature TimeChangeEvent
echo 	set time manually to 04.12.2011 0000
echo 	iTimedReboot2.exe 
ECHO ++++++++++++++ TEST10 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
echo ###	simululates a premature TimeChangeEvent  >>test_run.txt
echo set time manually to 04.12.2011 0000  >>test_run.txt
echo 	iTimedReboot2.exe   >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201112040000
CALL :MYWAIT
prun \iTimedReboot2.exe -test 
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 10 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST10  --------------- >>test_run.txt
ECHO .>>test_run.txt

ECHO TEST11...
echo 	simululates another premature TimeChangeEvent
echo 	set time manually to 01.12.2011 0100
echo 	iTimedReboot2.exe 
ECHO ++++++++++++++ TEST10 ++++++++++++++++ >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
echo ###	simululates a past time change  >>test_run.txt
echo set time manually to 01.12.2011 0100  >>test_run.txt
echo 	iTimedReboot2.exe   >>test_run.txt
ECHO ------------------------------------- >>test_run.txt
prun \SetDateTime.exe 201112010100
CALL :MYWAIT
prun \iTimedReboot2.exe -test 
ECHO "############# Result:" >>test_run.txt
pregutl HKLM\Software\Intermec\iTimedReboot2 >>test_run.txt
ECHO *************** LOG test 11 **************** >>test_run.txt
pget -f \iTimedReboot2.exe.log.txt
type iTimedReboot2.exe.log.txt >>test_run.txt
pdel \iTimedReboot2.exe.log.txt
ECHO ---------------TEST11  --------------- >>test_run.txt
ECHO .>>test_run.txt

:END_TESTS
findstr /g:find_start.txt iTimedReboot2.exe.log.txt
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

:MYEND
@echo on
