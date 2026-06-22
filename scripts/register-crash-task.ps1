<#
.SYNOPSIS
  Register (or remove) the Scheduled Task that runs process-worldserver-crashes.ps1.

.DESCRIPTION
  Uses schtasks.exe so it works WITHOUT elevation: the task runs as the current
  user, only while logged on (so the Docker Desktop / docker CLI session is
  available), every N minutes. (Register-ScheduledTask via the CIM provider
  requires an elevated shell to write the task root; schtasks does not.)

.EXAMPLE
  powershell -File scripts\register-crash-task.ps1
  powershell -File scripts\register-crash-task.ps1 -Remove
#>
[CmdletBinding()]
param(
    [string]$TaskName = 'TrinityCore-WorldserverCrashBacktraces',
    [int]   $EveryMin = 15,
    [switch]$Remove
)

$script = Join-Path $PSScriptRoot 'process-worldserver-crashes.ps1'

if ($Remove) {
    schtasks /Delete /TN $TaskName /F
    return
}

if (-not (Test-Path -LiteralPath $script)) { throw "Processor script not found: $script" }

# Launch via a hidden VBScript wrapper, NOT powershell.exe directly: a scheduled task
# running powershell flashes a console window every interval even with -WindowStyle Hidden.
# wscript + WScript.Shell.Run(intWindowStyle=0) starts it with no window at all.
$vbs = Join-Path $PSScriptRoot 'run-crash-hidden.vbs'
if (-not (Test-Path -LiteralPath $vbs)) { throw "Hidden launcher not found: $vbs" }
$tr = "wscript.exe $vbs"    # NB: paths here must be space-free (they are)

# /SC MINUTE /MO N => run every N minutes while the user is logged on (no stored
# password, no elevation). Covers "soon after logon" within one interval.
schtasks /Create /TN $TaskName /TR $tr /SC MINUTE /MO $EveryMin /F
if ($LASTEXITCODE -eq 0) {
    "Registered '$TaskName' (every $EveryMin min, current user, while logged on)."
    "Run now:  schtasks /Run /TN $TaskName"
    "Remove:   powershell -File scripts\register-crash-task.ps1 -Remove"
} else {
    "schtasks failed (exit $LASTEXITCODE)."
}
