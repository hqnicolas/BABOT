param(
    [string]$Fqbn = "esp32:esp32:esp32s3",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$arduinoCli = "C:\Program Files\Arduino CLI\arduino-cli.exe"
$scriptsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptsRoot
$sketchRoot = Join-Path $repoRoot "firmware"
$manifestPath = Join-Path $repoRoot "docs\third-party-sources.md"
$extraFlags = @(
    "-I$sketchRoot\\src"
    "-I$sketchRoot\\src\\third_party\\ArduinoJson\\src"
    "-I$sketchRoot\\src\\third_party\\Adafruit_NeoPixel"
    "-I$sketchRoot\\src\\third_party\\CD74HC4067\\src"
    "-I$sketchRoot\\src\\third_party\\Servo\\src"
    "-I$sketchRoot\\src\\third_party\\WebSockets\\src"
) -join " "

$args = @(
    "compile"
    "--fqbn", $Fqbn
    "--build-property", "build.extra_flags=$extraFlags"
)

if ($Clean) {
    $args += "--clean"
}

if (Test-Path $manifestPath) {
    Write-Host "Using vendored libraries described in $manifestPath"
}

$args += $sketchRoot

& $arduinoCli @args
