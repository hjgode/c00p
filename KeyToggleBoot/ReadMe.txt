= History and usage KeyToggleBoot =

== Function ==

Define a key sequence to bring up a dialog to enable the user to Shutdown/Reboot a CN50

== Configuration ==

Configuration is done by registry
{{{
				REGEDIT4

				[HKEY_LOCAL_MACHINE\Software\Intermec\KeyToggleBoot]
				"ForbiddenKeys"=hex:\
					  00
					list of vk_ values of forbidden keys, these key values are not forwarded from the hook
					
				"RebootText"="Neustart"
					text on the Reboot button
				"QuestionText"="Sie haben den Reboot-Dialog geöffnet. Was möchten Sie?"
					text for the message
				"CancelText"="Abbrechen"
					text for the cancel button
				"ShutdownText"="Herunterfahren"
					text for the shutdown button
				"SuspendText"="Standby"
					text for the standby button
				"KeySeq"="***"
					char sequence to trigger the app, default="..."
				"Timeout"=dword:00000003
					time allowed to enter char sequence
				"LEDid"=dword:00000001
					ID of the LED to blink when the sequence is triggered, not all LED IDs support blinking!
					use 0 on CN51	(left yellow LED)
					use 1 on CN50
					default=1 !
					
				"ShutdownExt"=dword:00000001
					enable extern Shutdown handling
				"ShutdownExtApp"="\\windows\\iexplore.exe"
					app to execute for extern shutdown handling
				"ShutdownExtParms"="http://www.google.com"
					args for extern shutdown app
				"ShowShutdownButton"=dword:00000000
					1 = show Shutdown button
					0 = hide Shutdown button (default, also is reg is not found)

				"RebootExt"=dword:00000001
					enable extern Reboot handling
				"RebootExtApp"="\\windows\\ctlpnl.exe"
					app to execute for extern reboot handling
				"RebootExtParms"="cplmain.cpl 16"
					args for extern reboot application
					
				"ShowSuspendButton"=dword:00000000
					1 = show Suspend button
					0 = hide Suspend button (default, also is reg is not found)

				"EnableAlarm"=dword:00000000
					0 = Alarm after idle timeout is disabled (default)
					1 = Alarm after idle timeout is enabled
				"IdleTimeout"=dword:0000012c
					amount of seconds for timeout alarm. default = 300 seconds = 5 minutes
				"AlarmOffKey"=dword:00000073
					key to shutoff alarm, default 0x73
				
				"InfoEnabled"=dword:00000001
					show/supress info dialog, default=1
				"InfoText"="Idle time elapsed alarm!"
				"InfoButton1"="Snooze"
					this button will reset the idle timer
				"InfoButton2"="Dismiss"
					this button will hide the info dialog until next alarm

}}}

== Command Line arguments ==

You can use "KeyToggleBoot.exe -writereg" to get the default 
registry entries written to the registry

== Hardware ==

Tested with 700 color and CN3 / WM5, CN50 / WM 6.1

== Debug notes ==

	In DEBUG mode the alarm off key is VK_0
	AC power is not checked for alarm
	Idle timeout is fixed to 5 seconds
	DoAlarm uses simple MessageBeep calls instead of ITC_PlayAudioTone

== History ==

  3.5.0
			added ShowShutdownButton option
			added blinking LED on alarm condition, same LED used as for key sequence indicator
			note: not all LED support blink option!
			
			new info dialog, registry options
			InfoEnabled		DWORD	1							show/supress info dialog
			InfoText		text	"Idle time elapsed alarm!"
			InfoButton1		text	"Snooze"					this button will reset the idle timer
			InfoButton2		text	"Dismiss"					this button will hide the info dialog until next alarm
			
  3.4.2		added Alarm Off key customizations:
				"AlarmOffKey"=dword:00000073
					key to shutoff alarm, default 0x73
                                        
  3.4.1		added new thread to watch for AC power (interval 1 second)
			if AC powered, the beeperThread is stopped and idleThread is reset (starts a new interval)
			if not AC powered there is no change
			
			alarm sound is now issued directly and then each 5 seconds. Before it was issued first 
			after 5 seconds timeout with interval of 5 seconds
			
  3.4.0		added FindMe alarm function
			if alarm is enabled by registry, the device will issue alarm beeps after the idle 
			timeout (default 5 minutes). Every key release restarts the idle timeout.
			If device is on AC power, there will be no alarm, the idle time will simply restart.
			If alarm is active, only the HangUp (F3, PhoneEnd) key will stop the alarm. Other keys
			have no effect on the already active alarm.
			
  3.3.5		explicit check for 0 or 1 (nothing else matters) in CN50_Shutdown() and ResetPocketPC()
  
  3.3.5		fixed ReadReg for ...Parms reading
			changed event name and vars for Suspend action
			added ShowSuspendButton in reg, default is to hide Suspend button
			
  3.3.4		changed rebootdlg.cpp to close itself if shutdown or reboot button has been tapped
			fixed logic and moved regValShutdownExt and regValRebootExt logic from hook proc to
			rebootdlg.cpp
			added code to read version information from central point at VERSION_INFO resource

  3.3.3		added registry keys
			"ShutdownExt"
			"ShutdownExtApp"
			"ShutdownExtParms"

  
  3.3.2		added registry keys
				RebootExt
				RebootExtApp
				RebootExtParms
			added code to start external app instead of showing internal dialog

  3.3.1		changed reboot code:
			BOOL ResetPocketPC()
			{
				SetSystemPowerState(NULL, POWER_STATE_RESET, 0);
				return 0;
				//DWORD dwRet=0;
				//return KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, &dwRet);
			}
			For whatever reason KernelIOCtl does not work anymore

  3.3.0		added code to read button texts from Registry
				REGEDIT4

				[HKEY_LOCAL_MACHINE\Software\Intermec\KeyToggleBoot]
				"ForbiddenKeys"=hex:\
					  00
				"RebootText"="Neustart"
				"QuestionText"="Sie haben den Reboot-Dialog geöffnet. Was möchten Sie?"
				"CancelText"="Abbrechen"
				"ShutdownText"="Herunterfahren"
				"SuspendText"="Standby"
				"KeySeq"="***"
				"Timeout"=dword:00000003
				"LEDid"=dword:00000001
			font in dialog changed to Tahoma 8pt and resized buttons
			changed registry.h and splitted to registry.h and cpp

  3.2.0		imported to VS2008 and fixed some pointer bugs
			added Suspend option

  3.1.1		fixed pForbiddenKeyList size recognition by adding 0x00 as end marker
			cannot use sizeof(pForbiddenKeyList) as this returns size of pointer

  3.1.0		added ForbiddenKeyList to code and registry
                [HKEY_LOCAL_MACHINE\Software\Intermec\KeyToggleBoot]
				"KeySeq"=string:"*.#"
                "LEDid"=dword:00000001
                "Timeout"=dword:00000003
				"ForbiddenKeys"=hex:\
							72,73
			ForbiddenKeys is a binary list of byte codes. These byte codes define 
			the VK values to be catched and consumed by KeyToggleBoot
			in example, the VK code for F3 and F4 is 0x72 and 0x73. So KeyToggleBoot 
			will not forward these two keys, neither WM_KEYDOWN nor WM_KEYUP will be
			send.

  3.0.0		first release of coding to KeyToggleBoot
			added LEDid code
			LEDid defines the ID of the LED to use for showing sticky state
                [HKEY_LOCAL_MACHINE\Software\Intermec\KeyToggleBoot]
				"KeySeq"=string:"*.#"
                "LEDid"=dword:00000001
                "Timeout"=dword:00000003

  2.1.1		~
			changed code to catch sticky key
  2.1		+
			Added Registry code to read vars from Reg
			[HKEY_LOCAL_MACHINE\SOFTWARE\Intermec\KeyToggleBoot]
			StickyKey = DWORD	defines the VK_ Value for the Sticky Key, default is VK_NUMLOCK is 0x90
			Timeout = DWORD		defines the time the sticky key is active, specify in seconds!, default is 3
			autoFallback = DWORD	defines, if the sticky state is reset after a number key is pressed
			+
			Added code for LEDs
			-
  2.0		initial release, started from iHook3


