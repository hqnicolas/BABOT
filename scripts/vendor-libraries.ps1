$ErrorActionPreference = "Stop"

$arduinoCli = "C:\Program Files\Arduino CLI\arduino-cli.exe"
$scriptsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptsRoot
$stagingRoot = Join-Path $repoRoot ".vendor-staging"
$stagingSketchbook = Join-Path $stagingRoot "sketchbook"
$stagingLibraries = Join-Path $stagingSketchbook "libraries"
$stagingData = Join-Path $stagingRoot "data"
$stagingDownloads = Join-Path $stagingRoot "downloads"
$configFile = Join-Path $stagingRoot "arduino-cli.yaml"
$vendorRoot = Join-Path $repoRoot "firmware\src\third_party"
$manifestPath = Join-Path $repoRoot "docs\third-party-sources.md"

$libraries = @(
    @{
        DisplayName = "Servo"
        InstallRef = "Servo"
        Folder = "Servo"
        Upstream = "https://github.com/arduino-libraries/Servo"
        RetainedLicenses = @("docs", "examples")
        CopyItems = @("src", "docs", "examples")
    },
    @{
        DisplayName = "CD74HC4067"
        InstallRef = "CD74HC4067@1.0.2"
        Folder = "CD74HC4067"
        Upstream = "https://github.com/waspinator/CD74HC4067"
        RetainedLicenses = @("LICENSE")
        CopyItems = @("src", "LICENSE")
    },
    @{
        DisplayName = "Adafruit NeoPixel"
        InstallRef = "Adafruit NeoPixel@1.15.4"
        Folder = "Adafruit_NeoPixel"
        Upstream = "https://github.com/adafruit/Adafruit_NeoPixel"
        RetainedLicenses = @("COPYING")
        CopyItems = @("Adafruit_NeoPixel.cpp", "Adafruit_NeoPixel.h", "esp.c", "rp2040_pio.h", "COPYING")
    },
    @{
        DisplayName = "WebSockets"
        InstallRef = "WebSockets@2.7.2"
        Folder = "WebSockets"
        Upstream = "https://github.com/Links2004/arduinoWebSockets"
        RetainedLicenses = @("LICENSE")
        CopyItems = @("src", "LICENSE")
    },
    @{
        DisplayName = "Async TCP"
        InstallRef = "Async TCP@3.3.2"
        Folder = "AsyncTCP"
        Upstream = "https://github.com/me-no-dev/asynctcp"
        RetainedLicenses = @("LICENSE")
        CopyItems = @("src", "LICENSE")
    },
    @{
        DisplayName = "ESP Async WebServer"
        InstallRef = "ESP Async WebServer@3.6.0"
        Folder = "ESPAsyncWebServer"
        Upstream = "https://github.com/me-no-dev/espasyncwebserver"
        RetainedLicenses = @("LICENSE")
        CopyItems = @("src", "LICENSE")
    },
    @{
        DisplayName = "ArduinoJson"
        InstallRef = "ArduinoJson@7.4.3"
        Folder = "ArduinoJson"
        Upstream = "https://github.com/bblanchon/ArduinoJson"
        RetainedLicenses = @("LICENSE.txt")
        CopyItems = @("src", "ArduinoJson.h", "LICENSE.txt")
    }
)

function Copy-NormalizedItem {
    param(
        [string]$SourceRoot,
        [string]$DestinationRoot,
        [string]$RelativePath
    )

    $source = Join-Path $SourceRoot $RelativePath
    $destination = Join-Path $DestinationRoot $RelativePath
    if (Test-Path $source -PathType Container) {
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Copy-Item -Path (Join-Path $source "*") -Destination $destination -Recurse -Force
        return
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

try {
    if (Test-Path $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }

    New-Item -ItemType Directory -Path $stagingLibraries -Force | Out-Null
    Set-Content -LiteralPath $configFile -Value @(
        "directories:"
        "  data: $stagingData"
        "  downloads: $stagingDownloads"
        "  user: $stagingSketchbook"
    )

    & $arduinoCli "--config-file" $configFile "lib" "install" ($libraries.InstallRef)

    if (-not (Test-Path $vendorRoot)) {
        New-Item -ItemType Directory -Path $vendorRoot | Out-Null
    }

    foreach ($library in $libraries) {
        $sourceRoot = Join-Path $stagingLibraries $library.Folder
        $destinationRoot = Join-Path $vendorRoot $library.Folder

        if (Test-Path $destinationRoot) {
            Remove-Item -LiteralPath $destinationRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Path $destinationRoot | Out-Null

        foreach ($item in $library.CopyItems) {
            Copy-NormalizedItem -SourceRoot $sourceRoot -DestinationRoot $destinationRoot -RelativePath $item
        }
    }

    $arduinoJsonCmake = Join-Path $vendorRoot "ArduinoJson\src\CMakeLists.txt"
    if (Test-Path $arduinoJsonCmake) {
        Remove-Item -LiteralPath $arduinoJsonCmake -Force
    }

    $arduinoJsonProjectedPaths = @(
        (Join-Path $repoRoot "firmware\ArduinoJson.h"),
        (Join-Path $repoRoot "firmware\ArduinoJson.hpp"),
        (Join-Path $repoRoot "firmware\ArduinoJson")
    )
    foreach ($path in $arduinoJsonProjectedPaths) {
        if (Test-Path $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }

    Copy-NormalizedItem -SourceRoot (Join-Path $stagingLibraries "ArduinoJson") -DestinationRoot (Join-Path $repoRoot "firmware") -RelativePath "src\ArduinoJson.h"
    Copy-NormalizedItem -SourceRoot (Join-Path $stagingLibraries "ArduinoJson") -DestinationRoot (Join-Path $repoRoot "firmware") -RelativePath "src\ArduinoJson.hpp"
    Copy-NormalizedItem -SourceRoot (Join-Path $stagingLibraries "ArduinoJson") -DestinationRoot (Join-Path $repoRoot "firmware") -RelativePath "src\ArduinoJson"

    $rootProjectedPaths = @(
        @{ SourceRoot = (Join-Path $stagingLibraries "Adafruit_NeoPixel"); RelativePath = "Adafruit_NeoPixel.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "CD74HC4067"); RelativePath = "src\CD74HC4067.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "Servo"); RelativePath = "src\Servo.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\WebSockets.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\WebSockets4WebServer.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\WebSocketsClient.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\WebSocketsNetworkClient.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\WebSocketsNetworkClientSecure.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\WebSocketsServer.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\WebSocketsVersion.h"; DestinationRoot = (Join-Path $repoRoot "firmware") },
        @{ SourceRoot = (Join-Path $stagingLibraries "WebSockets"); RelativePath = "src\SocketIOclient.h"; DestinationRoot = (Join-Path $repoRoot "firmware") }
    )
    foreach ($projection in $rootProjectedPaths) {
        $destination = Join-Path $projection.DestinationRoot (Split-Path -Leaf $projection.RelativePath)
        if (Test-Path $destination) {
            Remove-Item -LiteralPath $destination -Force
        }
        Copy-NormalizedItem -SourceRoot $projection.SourceRoot -DestinationRoot $projection.DestinationRoot -RelativePath $projection.RelativePath
    }

    $manifestLines = @(
        "# Third-Party Sources",
        "",
        "This repository vendors the build-relevant portions of its third-party Arduino dependencies under `firmware/src/third_party/`.",
        "",
        "| Dependency | Version | Upstream | Retained license files |",
        "|---|---|---|---|"
    )

    foreach ($library in $libraries) {
        $licenseCell = if ($library.RetainedLicenses.Count -gt 0) { ($library.RetainedLicenses -join ", ") } else { "None shipped upstream" }
        $manifestLines += "| $($library.DisplayName) | $($library.InstallRef.Split('@')[1]) | $($library.Upstream) | $licenseCell |"
    }

    $manifestLines += ""
    $manifestLines += "Refresh with `.\scripts\vendor-libraries.ps1`, then verify with `arduino-cli compile --fqbn esp32:esp32:esp32s3 .\firmware` or `.\scripts\compile.ps1`."
    Set-Content -LiteralPath $manifestPath -Value $manifestLines
}
finally {
    if (Test-Path $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
