$ErrorActionPreference = "Stop"

$Root = (Split-Path -Parent $MyInvocation.MyCommand.Path) -replace '\\', '/'
$WslRoot = (& wsl.exe wslpath -a "$Root").Trim()
$Command = "cd '$WslRoot' && bash scripts/fetch.sh && bash scripts/build.sh"

& wsl.exe -e bash -lc $Command
exit $LASTEXITCODE
