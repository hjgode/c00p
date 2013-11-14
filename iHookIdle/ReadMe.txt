========================================================================
    WIN32 APPLICATION : iHookIdle Project Overview
========================================================================

iHookIdle (based on KeyToggleBoot)

	1.
	watch keyboard and barcode scan events
	if not action within timeout, issue an alarm (LED, vibrate, showMessage)
	
	2.
	if keys pressed in special sequence, set the iLock5 stop event
	
config:
	primary functionality
	
		Alarm settings
			LEDid			LED to light on alarm
			VibrateID		ID of the vibrate 'LED'
			EnableAlarm		should we ever issue an alarm after an idle timeout?
			IdleTimeout		idle time before alarm is issued
			InfoText		text to show in alarm message
			InfoButton1		text on the dismiss-button (which just hides the message temporary)
			InfoButton2		text on the snooze-button (hide the msg and reset the idle time)
			InfoEnabled		enable or disable to alarm msg to be shown
			
			AlarmOffKey		single VK value to switch alarm off and reset idle time
	
	general settings
		ForbiddenKeys	binary array with list of VK values that are filtered from the OS
	
	secondary functionality
	
		exit iLock5 by keypad
			KeySeq			a string with chars that will issue the iLock5 stop event
			Timeout			time span in which KeySeq has to be entered

	REGEDIT4		
	[HKEY_LOCAL_MACHINE\Software\Intermec\iHookIdle]
	"LEDid"=dword:00000007
	"VibrateID"=dword:00000005
	"EnableAlarm"=dword:00000001
	"IdleTimeout"=dword:0000012C
	"InfoText"="Idle time elapsed alarm!"
	"InfoButton2"="Dismiss"
	"InfoButton1"="Snooze"
	"InfoEnabled"=dword:00000001
	"AlarmOffKey"=dword:00000073
	"ForbiddenKeys"=hex:\
		  72,73
	"KeySeq"="..."
	"Timeout"=dword:0000000A

history:
	v 3.6.0
		initial version based on KeyToggleBoot
		

/////////////////////////////////////////////////////////////////////////////