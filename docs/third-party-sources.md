# Third-Party Sources

This repository vendors the build-relevant portions of its third-party Arduino dependencies under `firmware/src/third_party/`.

| Dependency | Version | Upstream | Retained license files |
|---|---|---|---|
| Servo | current vendored copy | https://github.com/arduino-libraries/Servo | docs/ + examples/ included upstream |
| CD74HC4067 | 1.0.2 | https://github.com/waspinator/CD74HC4067 | LICENSE |
| WebSockets | 2.7.2 | https://github.com/Links2004/arduinoWebSockets | LICENSE |
| Async TCP | 3.3.2 | https://github.com/me-no-dev/asynctcp | LICENSE |
| ESP Async WebServer | 3.6.0 | https://github.com/me-no-dev/espasyncwebserver | LICENSE |
| ArduinoJson | 7.4.3 | https://github.com/bblanchon/ArduinoJson | LICENSE.txt (`firmware/src/third_party/ArduinoJson/src`) |

Refresh with `.\scripts\vendor-libraries.ps1`, then verify with `arduino-cli compile --fqbn esp32:esp32:esp32s3 .\firmware` or `.\scripts\compile.ps1`.
