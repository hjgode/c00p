========================================================================
       Windows CE APPLICATION : iTimedReboot2
========================================================================


===purpose===
	v1 based
	watch an IP to ping, changes icon on today screen for number of returned pings
	reboot at a specified time
	
	v2
	on-reboot-action can now be handled by external app
		set "RebootExt" to string="1"
		set "RebootExtApp" to string with name of the executable to start
		set "RebootExtParms" to string with args to be used for exectable specified with "RebootExtApp" 
	if "RebootExt":string="0" iTemedReboot will work as v1 version and initiate a reboot
	
===notes===
	as iTimedReboot uses timers it will not run, when a device is in suspend mode. In suspend mode only hardware
	or notification events can wake up a device
	
===logging===
	if enabled, a log will be written to "\iTimedReboot2.log.txt"
	
===Registry settings===
	You have to specify ALL settings!
	All settings are strings (REG_SZ)
	Main key
		HKLM,"SOFTWARE\Intermec\iTimedReboot2"
	names and values
		"LastBootDate","20000101"
			controls if reboot will take place, iTimedReboot does not initiate a second reboot on same date
			date string is in format YYYYMMDD
		"RebootTime","02:00"
			define the time of when to reboot, uses 24 hours format
		"PingTarget","10.1.104.228"
			define the IP address of the host to ping periodically
		"PingInterva,"600"
			define how often to test the host
			if 0, there will be no periodcal ping 
		"Interva,"60"
			define how often to look at the time
			if 0 there will be no time check and we will never boot or excute an exe
		"EnableLogging","0"
			"1" = enable logging to file
			"0" = no logging to file
		"RebootExt","1" 
			"0" to initiate a reboot
			"1" to run external exe
		"RebootExtApp","\Storage Card\APP\KillAndReboot.exe"
			complete path and name of external exe
		"RebootExtParms",""
			arguments to be used when starting external exe

===default registry===
	if started with arg "-writereg" a default registry will be written:
		"Interval="30"
		"RebootTime="00:00"
		"PingInterval"="60"
		"PingTarget"="127.0.0.1"
		"EnableLogging"="1"
		"LastBootDate"="19800101"
		//new with v2
		"RebootExt"="1"
		"RebootExtApp"="\Windows\fexplore.exe"
		"RebootExtParms"="\Flash File Store"
