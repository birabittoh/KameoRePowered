# Resolve XEX import stubs for ReKameo.
#
# Reads:
#   - tools/x360_imports.idc   (ordinal -> kernel/xam export name)
#   - out/default.xex.idc      (stub address -> ordinal)
#
# Writes:
#   - out/kameo_imports.csv    (resolved: address, lib, ordinal, name, kind)

param(
    [string]$XexImports  = "$PSScriptRoot/../out/default.xex.idc",
    [string]$NamesIdc    = "$PSScriptRoot/x360_imports.idc",
    [string]$OutCsv      = "$PSScriptRoot/../out/kameo_imports.csv"
)

function Parse-NameTable {
    param([string]$IdcPath, [string]$LibName)

    $block = Get-Content -Raw -LiteralPath $IdcPath
    # Each library has its own "<lib>NameGen" function; grab only that block.
    $start = $block.IndexOf("static ${LibName}NameGen(")
    if ($start -lt 0) { throw "Block for $LibName not found in $IdcPath" }
    $end = $block.IndexOf("`nstatic ", $start + 1)
    if ($end -lt 0) { $end = $block.Length }
    $body = $block.Substring($start, $end - $start)

    $map = @{}
    $regex = [regex]'id\s*==\s*0x([0-9A-Fa-f]+)\)\s*funcName\s*=\s*"([^"]+)"'
    foreach ($m in $regex.Matches($body)) {
        $ord = [uint32]("0x" + $m.Groups[1].Value)
        $map[$ord] = $m.Groups[2].Value
    }
    return $map
}

$krnl = Parse-NameTable -IdcPath $NamesIdc -LibName 'xboxkrnl'
$xam  = Parse-NameTable -IdcPath $NamesIdc -LibName 'xam'

Write-Host "Loaded xboxkrnl names: $($krnl.Count)"
Write-Host "Loaded xam      names: $($xam.Count)"

$stubRegex = [regex]'SetupImport(Func|Data)\(\s*0x([0-9A-Fa-f]+)\s*,\s*(?:0x([0-9A-Fa-f]+)\s*,\s*)?0x([0-9A-Fa-f]+)\s*,\s*"([^"]+)"\s*\)\s*;'

$rows = New-Object System.Collections.Generic.List[object]
$lines = Get-Content -LiteralPath $XexImports
foreach ($line in $lines) {
    $m = $stubRegex.Match($line)
    if (-not $m.Success) { continue }

    $kind       = $m.Groups[1].Value              # Func or Data
    $importAddr = [uint32]("0x" + $m.Groups[2].Value)
    $funcAddr   = 0
    if ($m.Groups[3].Success -and $m.Groups[3].Value) {
        $funcAddr = [uint32]("0x" + $m.Groups[3].Value)
    }
    $ordinal    = [uint32]("0x" + $m.Groups[4].Value)
    $lib        = $m.Groups[5].Value

    $libShort = ($lib -split '\.')[0]
    $name = switch ($libShort) {
        'xboxkrnl' { if ($krnl.ContainsKey($ordinal)) { $krnl[$ordinal] } else { "xboxkrnl_{0:X8}" -f $ordinal } }
        'xam'      { if ($xam.ContainsKey($ordinal))  { $xam[$ordinal]  } else { "xam_{0:X8}"      -f $ordinal } }
        default    { "${libShort}_{0:X8}" -f $ordinal }
    }

    $rows.Add([pscustomobject]@{
        StubAddress    = ("0x{0:X8}" -f $importAddr)
        FuncAddress    = if ($funcAddr) { "0x{0:X8}" -f $funcAddr } else { "" }
        Library        = $libShort
        Ordinal        = ("0x{0:X}" -f $ordinal)
        Kind           = $kind
        Name           = $name
    }) | Out-Null
}

$rows | Sort-Object { [uint32]$_.StubAddress } | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutCsv

Write-Host "Wrote $($rows.Count) rows to $OutCsv"

# quick summary: how many resolved / unresolved per library
$rows | Group-Object Library | ForEach-Object {
    $total = $_.Count
    $named = ($_.Group | Where-Object { $_.Name -notmatch '^(xboxkrnl_|xam_|syscall_)' }).Count
    Write-Host ("  {0,-10} {1}/{2} resolved" -f $_.Name, $named, $total)
}
