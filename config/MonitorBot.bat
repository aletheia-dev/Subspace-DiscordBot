@echo off
:Start
"C:\Program Files (x86)\Continuum\DiscordBot\DiscordBot.exe"
:: Wait 60 seconds before restarting.
IF NOT EXIST "C:\Program Files (x86)\Continuum\DiscordBot\STOP_DISCORD_BOT" (
	TIMEOUT /T 60
	GOTO:Start)
