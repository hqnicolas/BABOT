# Hardware Guide

This repository separates hardware source files, fabrication exports, enclosure assets, and shared KiCad libraries into dedicated folders.

## Hardware Layout

- [`../hardware/boards/main-board/`](../hardware/boards/main-board/) - main-board KiCad source files
- [`../hardware/boards/led-board/`](../hardware/boards/led-board/) - LED-board KiCad source files
- [`../hardware/fabrication/main-board/`](../hardware/fabrication/main-board/) - main-board fabrication and exported reference PDFs
- [`../hardware/fabrication/led-board/`](../hardware/fabrication/led-board/) - LED-board fabrication and exported reference PDFs
- [`../hardware/enclosure/`](../hardware/enclosure/) - 3D-printable enclosure STL files and mechanical reference assets
- [`../hardware/libraries/kicad/`](../hardware/libraries/kicad/) - shared KiCad symbols and footprints
- [`../hardware/reference/tm.pdf`](../hardware/reference/tm.pdf) - additional hardware reference PDF

## Notes

- Board source CAD and fabrication exports are intentionally separated so manufacturing files do not clutter the editable design directories.
- STL filenames are preserved where they appear to be fabrication-ready outputs from the original project.
- The photo asset under `hardware/enclosure/` is kept with the STL files because it documents the printed assembly.
