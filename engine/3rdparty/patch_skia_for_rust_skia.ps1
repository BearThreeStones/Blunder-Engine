# Patches a rust-skia/skia m142 offline tree so skia-bindings 0.90 + current gn can run on Windows.
# Usage: patch_skia_for_rust_skia.ps1 -SkiaRoot D:\sk

param(
  [Parameter(Mandatory = $true)]
  [string]$SkiaRoot
)

$ErrorActionPreference = "Stop"
$SkiaRoot = $SkiaRoot.TrimEnd('\', '/')

if (-not (Test-Path "$SkiaRoot\DEPS")) {
  Write-Error "Not a Skia tree: $SkiaRoot"
}

$gn = "$SkiaRoot\bin\gn.exe"
if (-not (Test-Path $gn)) {
  Write-Host "Fetching gn via bin/fetch-gn ..."
  Push-Location $SkiaRoot
  python "$SkiaRoot\bin\fetch-gn"
  Pop-Location
  if (-not (Test-Path $gn)) {
    Write-Error "Missing $gn after fetch-gn"
  }
}

$thirdPartyGni = "$SkiaRoot\third_party\third_party.gni"
$gni = Get-Content $thirdPartyGni -Raw
$gni = $gni -replace '(?m)^      configs \+= \[ "\.\./\.\./gn/skia:recover_pointer_overflow" \]', '      # configs += [ "../../gn/skia:recover_pointer_overflow" ]'
Set-Content $thirdPartyGni $gni -NoNewline

$buildGn = "$SkiaRoot\BUILD.gn"
$bg = Get-Content $buildGn -Raw
if ($bg -match 'config\("skia_private"\) \{\s+visibility = ') {
  $bg = $bg -replace 'config\("skia_private"\) \{\s+visibility = \[[^\]]*\]\s*', "config(`"skia_private`") {`n"
  Set-Content $buildGn $bg -NoNewline
}

Write-Host "Patched $SkiaRoot for rust-skia Windows builds."
