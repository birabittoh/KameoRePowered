# Dump raw PPC instructions around a guest virtual address.
#
# We don't have a full PPC disassembler handy, but for diagnosis we just
# need to see the raw big-endian 4-byte words and decode a few common
# forms (branch family, blr, nop).

param(
    [Parameter(Mandatory)][string]$Address,
    [int]$BytesBefore = 0x20,
    [int]$BytesAfter  = 0x40,
    [string]$Bin      = "$PSScriptRoot/../out/default.xex.bin",
    [string]$Base     = "0x82000000"
)

function Parse-UInt32([string]$s) {
    if ($s.StartsWith('0x') -or $s.StartsWith('0X')) {
        return [Convert]::ToUInt32($s.Substring(2), 16)
    }
    return [Convert]::ToUInt32($s, 16)
}

[uint32]$addr = Parse-UInt32 $Address
[uint32]$base = Parse-UInt32 $Base

$fileOffset = ($addr - $base) - $BytesBefore
$length     = $BytesBefore + $BytesAfter

$bytes = [System.IO.File]::ReadAllBytes($Bin)
if ($fileOffset + $length -gt $bytes.Length) {
    throw ("Out of range: addr=0x{0:X8} offset=0x{1:X} file=0x{2:X}" -f $addr, $fileOffset, $bytes.Length)
}

function Disasm-PPC {
    param([uint32]$Addr, [uint32]$Word)

    if ($Word -eq 0x60000000) { return 'nop' }
    if ($Word -eq 0x4E800020) { return 'blr' }

    $opcode = ($Word -shr 26) -band 0x3F

    switch ($opcode) {
        18 {
            $li = $Word -band 0x03FFFFFC
            if ($li -band 0x02000000) { $li = $li -bor 0xFC000000 }
            $aa = ($Word -shr 1) -band 1
            $lk = $Word -band 1
            $target = if ($aa) { [uint32]$li } else { [uint32]([int64]$Addr + [int32]$li) }
            $mnem = switch ("${aa}${lk}") {
                '00' { 'b'   }
                '01' { 'bl'  }
                '10' { 'ba'  }
                '11' { 'bla' }
            }
            return ("{0,-5} 0x{1:X8}" -f $mnem, $target)
        }
        16 {
            $bd = $Word -band 0xFFFC
            if ($bd -band 0x8000) { $bd = $bd -bor 0xFFFF0000 }
            $bo = ($Word -shr 21) -band 0x1F
            $bi = ($Word -shr 16) -band 0x1F
            $aa = ($Word -shr 1) -band 1
            $lk = $Word -band 1
            $target = if ($aa) { [uint32]$bd } else { [uint32]([int64]$Addr + [int32]$bd) }
            $suffix = ""
            if ($lk) { $suffix += "l" }
            if ($aa) { $suffix += "a" }
            return ("bc{0,-3} {1},{2},0x{3:X8}" -f $suffix, $bo, $bi, $target)
        }
        19 {
            $xo = ($Word -shr 1) -band 0x3FF
            if ($xo -eq 16)  { return 'bclr'  }
            if ($xo -eq 528) { return 'bcctr' }
            return ("op19 xo={0}" -f $xo)
        }
        default {
            return ""
        }
    }
}

for ($i = 0; $i -lt $length; $i += 4) {
    $o = $fileOffset + $i
    if ($o + 4 -gt $bytes.Length) { break }
    $word = ([uint32]$bytes[$o] -shl 24) -bor ([uint32]$bytes[$o+1] -shl 16) -bor ([uint32]$bytes[$o+2] -shl 8) -bor [uint32]$bytes[$o+3]
    $pc   = [uint32]($addr - $BytesBefore + $i)
    $mnem = Disasm-PPC -Addr $pc -Word $word
    $marker = if ($pc -eq $addr) { '>>>' } else { '   ' }
    "{0} 0x{1:X8}: {2:X8}  {3}" -f $marker, $pc, $word, $mnem
}
