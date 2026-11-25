$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$gcc = Get-Command 'xtensa-esp32s3-elf-gcc.exe' -ErrorAction SilentlyContinue
$strip = Get-Command 'xtensa-esp32s3-elf-strip.exe' -ErrorAction SilentlyContinue
$size = Get-Command 'xtensa-esp32s3-elf-size.exe' -ErrorAction SilentlyContinue
if (-not $gcc -or -not $strip -or -not $size) {
    throw 'ESP32-S3 GCC tools not found. Open an ESP-IDF PowerShell environment and rerun this script.'
}
New-Item -ItemType Directory -Path build -Force | Out-Null
# source/text_pad.S must stay LAST: it rounds .text up to a multiple of 4 bytes.
$flags = @('-std=c99', '-Os', '-Wall', '-Wextra', '-Werror', '-mlongcalls', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-fno-jump-tables', '-fno-tree-loop-distribute-patterns', '-nostartfiles', '-nostdlib', '-fPIC', '-shared', '-e', 'app_main', '-fdata-sections', '-ffunction-sections', '-Wl,--gc-sections', '-fvisibility=hidden', '-Wl,--strip-all', '-Wl,--strip-debug', '-Wl,--strip-discarded', '-Wl,-u,invader_text_pad', 'source/tengu_knight.c', 'source/text_pad.S', '-o', 'build/tengu_knight.unstripped.elf')
& $gcc.Source @flags
if ($LASTEXITCODE -ne 0) { throw "Compiler failed: exit code $LASTEXITCODE" }
$stripFlags = @('--strip-unneeded', '--remove-section=.comment', '--remove-section=.dynamic', '--remove-section=.got.loc', '--remove-section=.xt.lit', '--remove-section=.xt.prop', '--remove-section=.xtensa.info', '-o', 'tengu_knight.elf', 'build/tengu_knight.unstripped.elf')
& $strip.Source @stripFlags
if ($LASTEXITCODE -ne 0) { throw "Strip failed: exit code $LASTEXITCODE" }
$textLine = & $size.Source '-A' 'tengu_knight.elf' | Where-Object { $_ -match '^\.text\s+(\d+)' }
if (-not $textLine) { throw 'Could not read the .text size from tengu_knight.elf' }
$null = "$textLine" -match '^\.text\s+(\d+)'
$textSize = [int]$Matches[1]
if ($textSize % 4 -ne 0) {
    Remove-Item 'tengu_knight.elf' -Force
    throw ".text is $textSize bytes, not a multiple of 4. Do not run this ELF."
}
Write-Host "Built tengu_knight.elf (.text = $textSize bytes, word-aligned)" -ForegroundColor Green
