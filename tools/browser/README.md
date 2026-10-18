# Browser asset converter

This static Page turns a selected DOS Jazz Jackrabbit folder into an install
ZIP. Users do not need Python or a separate renderer. Conversion runs in a
Web Worker in a standalone WebAssembly module with no imports. The selected
files and converted pack stay in the browser; the Page does not upload them.

The result is a ZIP laid out for the SD card:

```text
homebrews/Jazz.bin
homebrews/Jazz.jj
```

`Jazz.bin` is the project homebrew binary built by the main CI job. The Page
and matching converter module are published by the Pages workflow.

## Build

Build the converter module with Docker, then assemble the Page with the
homebrew binary from the firmware build:

```sh
tools/extractor/build.sh
tools/browser/build-page.sh /tmp/jazz-pages Jazz.bin tools/extractor/build/jazz-assets.wasm
```

The assembled directory is static and can be served from any web server. The
browser downloads `Jazz.bin` and the converter module from the same site; no
game data is sent to that server.
