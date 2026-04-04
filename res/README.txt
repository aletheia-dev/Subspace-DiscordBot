Requirements for Recording
--------------------------

* Make sure OBS is configured with shortcuts F9 (start rec) and F10 (stop rec)
* Make sure to start OBS as administrator


Startup
-------
Start DiscordBot as administrator via context menu on the desktop icon. DiscordBot will launch Subspace, go to the arena it has last been in and switch to the Subspace chat window. This startup procedure takes about 15 seconds.


Restart after a connection loss
-------------------------------
When Subspace loses the connection to the server, the bot will automatically quit out of Subspace and shut down the "DiscordBot" process. To restart the bot, simply proceed as described in 'Startup'.


Restart after a crash
---------------------
In rare cases the bot might crash without shutting down the "DiscordBot" process. In this case, when the bot is still running and cannot be closed with Ctrl+q, open the task manager by typing "ta" in the taskbar search entry (magnifying glass icon) and close the task "DiscordBot" manually.


Startup with automatic Recovery
-------------------------------
Start MonitorBot as administrator. MonitorBot will run the batch script DiscordBot.bat, which will restart DiscordBot after a waiting time of 1 minute when Subspace loses connection.


Shutdown
--------
Hit Ctrl+q to manually shut down Discobot.


Note
----
Make sure that the following conditions are met:
- In Display Options set 'Enter Messages' and 'Leave Messages' to 'Chat'
- Subspace has to run in windowed mode. Do not make changes to any Subspace options.
- The Subspace chat window is always shown AND active while the bot is running.
- No other windows are opened on the desktop.
