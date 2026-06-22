' Launches the worldserver crash-backtrace processor with NO visible window.
' A scheduled task that runs powershell.exe directly flashes a console window for a
' split second even with -WindowStyle Hidden; WScript.Shell.Run with intWindowStyle=0
' starts it fully hidden, so nothing flashes on screen.
Dim shell
Set shell = CreateObject("WScript.Shell")
shell.Run "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""E:\TrinityCore\scripts\process-worldserver-crashes.ps1""", 0, False
