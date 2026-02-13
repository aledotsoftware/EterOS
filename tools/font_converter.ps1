<#
.SYNOPSIS
Convierte un archivo de fuente raw (binario 8x16, 4096 bytes) a C array para eterOS.

.DESCRIPTION
Uso: .\font_converter.ps1 -InputFile path\to\font.f16 -OutputFile font.c

.EXAMPLE
.\font_converter.ps1 -InputFile A7100.F16 -OutputFile ..\kernel\drivers\video\font.c
#>

param(
    [Parameter(Mandatory = $true)]
    [string]$InputFile,

    [Parameter(Mandatory = $true)]
    [string]$OutputFile
)

$bytes = [System.IO.File]::ReadAllBytes($InputFile)

if ($bytes.Length -ne 4096) {
    Write-Warning "El archivo de entrada tiene $($bytes.Length) bytes. Se esperaban 4096 bytes (256 chars * 16 rows)."
    # Continuamos igual, pero avisamos
}

$sb = New-Object System.Text.StringBuilder
$sb.AppendLine("/* Fuente generada automáticamente por font_converter.ps1 */")
$sb.AppendLine("#include <types.h>")
$sb.AppendLine("")
$sb.AppendLine("/* 256 caracteres * 16 bytes = 4096 bytes total */")
$sb.AppendLine("const uint8_t font8x16[256 * 16] = {")

for ($i = 0; $i -lt $bytes.Length; $i++) {
    if ($i % 16 -eq 0) {
        $sb.Append("    /* Char $([int]($i / 16)) */ ")
    }
    
    $hex = "0x{0:X2}" -f $bytes[$i]
    $sb.Append("$hex")
    
    if ($i -lt $bytes.Length - 1) {
        $sb.Append(", ")
    }
    
    if (($i + 1) % 16 -eq 0) {
        $sb.AppendLine("")
    }
}

$sb.AppendLine("};")
[System.IO.File]::WriteAllText($OutputFile, $sb.ToString())

Write-Host "Convertido $InputFile a $OutputFile exitosamente." -ForegroundColor Green
