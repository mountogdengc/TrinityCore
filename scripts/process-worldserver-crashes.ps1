<#
.SYNOPSIS
  Convert WSL worldserver core dumps into small gdb backtraces, then delete the dumps.

.DESCRIPTION
  When the dockerised worldserver segfaults, the WSL crash handler
  (core_pattern = |/wsl-capture-crash) writes a multi-GB ELF core to
  %LOCALAPPDATA%\Temp\wsl-crashes\ named like:
      wsl-crash-<unixtime>-1-_opt_trinitycore_bin_worldserver-<signal>.dmp

  Those dumps are only useful as a backtrace, and only against the matching
  binary. This script runs gdb (from the trinitycore-debug:local image, which
  is FROM trinitycore:local so the binary/symbols match) against each dump,
  saves "thread apply all bt" to crash-backtraces\, and on success deletes the
  giant .dmp. The small text backtrace is the keepable artifact.

  Designed to be safe to run unattended (Scheduled Task): it never throws on a
  missing daemon / stale image, it only deletes a dump AFTER a non-trivial
  backtrace is written, and it appends a run log.

.NOTES
  Rebuild the debug image after rebuilding worldserver:
      docker build -t trinitycore-debug:local -f docker/debug/Dockerfile docker/debug/
#>
[CmdletBinding()]
param(
    [string]$CrashDir = (Join-Path $env:LOCALAPPDATA 'Temp\wsl-crashes'),
    [string]$OutDir   = 'E:\TrinityCore\crash-backtraces',
    [string]$Image    = 'trinitycore-debug:local',
    [string]$Binary   = '/opt/trinitycore/bin/worldserver',
    [switch]$KeepDump          # keep the .dmp even after a successful backtrace
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$logFile = Join-Path $OutDir 'process-crashes.log'

function Write-Log([string]$msg) {
    $line = "[{0}] {1}" -f (Get-Date -Format 's'), $msg
    $line | Tee-Object -FilePath $logFile -Append | Out-Host
}

# --- preconditions: docker reachable + debug image present (fail soft) ---
try { & docker info *> $null } catch { Write-Log "docker not reachable; skipping run."; return }
if ($LASTEXITCODE -ne 0) { Write-Log "docker not reachable (exit $LASTEXITCODE); skipping run."; return }

& docker image inspect $Image *> $null
$haveImage = ($LASTEXITCODE -eq 0)
if (-not $haveImage) {
    Write-Log "debug image '$Image' missing. Build it: docker build -t $Image -f docker/debug/Dockerfile docker/debug/"
    return
}

if (-not (Test-Path -LiteralPath $CrashDir)) { Write-Log "no crash dir yet ($CrashDir); nothing to do."; return }

$dumps = @(Get-ChildItem -LiteralPath $CrashDir -Filter '*worldserver*.dmp' -File -ErrorAction SilentlyContinue |
           Sort-Object LastWriteTime)
if ($dumps.Count -eq 0) { Write-Log "no worldserver dumps pending."; return }

Write-Log "found $($dumps.Count) worldserver dump(s) to process."
$ok = 0; $failed = 0

foreach ($dump in $dumps) {
    # wsl-crash-<ts>-1-_opt_trinitycore_bin_worldserver-<sig>.dmp
    $ts  = 'unknown'; $sig = 'NA'
    if ($dump.Name -match 'wsl-crash-(\d+)-.*-(\d+)\.dmp$') { $ts = $Matches[1]; $sig = $Matches[2] }
    $stamp   = try { ([DateTimeOffset]::FromUnixTimeSeconds([int64]$ts)).LocalDateTime.ToString('yyyyMMdd-HHmmss') } catch { $ts }
    $outName = "worldserver-crash-$stamp-sig$sig.bt.txt"
    $outPath = Join-Path $OutDir $outName

    Write-Log "analysing $($dump.Name) ($([math]::Round($dump.Length/1GB,2)) GB) -> $outName"

    # gdb runs inside the debug image; mount the dump's folder read-only at /dumps.
    $gdbArgs = @(
        'run','--rm',
        '-v', ("{0}:/dumps:ro" -f $dump.DirectoryName),
        $Image,
        'gdb','-batch','-nx',
        '-ex','set pagination off',
        '-ex','set print pretty on',
        '-ex','info registers',
        '-ex','thread apply all bt',
        $Binary, ("/dumps/{0}" -f $dump.Name)
    )

    $bt = & docker @gdbArgs 2>&1
    $gdbExit = $LASTEXITCODE

    $hasFrames = ($bt -join "`n") -match '(?m)^\s*#\d+\s'   # at least one stack frame "#0 ..."
    if ($gdbExit -eq 0 -and $hasFrames) {
        $header = @(
            "Worldserver crash backtrace",
            "  dump      : $($dump.Name)",
            "  crashed   : $stamp (local)   signal: $sig",
            "  binary    : $Binary  (image: $Image)",
            "  generated : $(Get-Date -Format 's')",
            ("=" * 72), ""
        ) -join "`r`n"
        Set-Content -LiteralPath $outPath -Value ($header + (($bt -join "`r`n"))) -Encoding utf8
        Write-Log "  -> backtrace saved ($([math]::Round((Get-Item $outPath).Length/1KB,1)) KB)."
        if (-not $KeepDump) {
            Remove-Item -LiteralPath $dump.FullName -Force -ErrorAction SilentlyContinue
            if (-not (Test-Path -LiteralPath $dump.FullName)) { Write-Log "  -> dump deleted (freed $([math]::Round($dump.Length/1GB,2)) GB)." }
            else { Write-Log "  -> WARNING: could not delete dump (locked?)." }
        }
        $ok++
    }
    else {
        # keep the dump; save gdb output for inspection so the run isn't a black box
        $errPath = Join-Path $OutDir "worldserver-crash-$stamp-sig$sig.gdb-FAILED.txt"
        Set-Content -LiteralPath $errPath -Value (($bt -join "`r`n")) -Encoding utf8
        Write-Log "  -> gdb FAILED (exit $gdbExit, no frames). Dump kept; gdb output: $errPath"
        $failed++
    }
}

Write-Log "done. $ok ok, $failed failed."
