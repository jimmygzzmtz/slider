# Slider

Slider is the browser port of Animal Crossing built from the decompilation codebase and compiled to WebAssembly with Emscripten. The web build renders through WebGL2, supports the browser shell for ROM selection and save mounts, and is designed to deploy as a static site on GitHub Pages or any Nginx-based web server.

This repository does not ship the game assets or the original ROM. You still need a legal copy of the Animal Crossing USA GameCube disc image to run the game.

Supported version: GAFE01_00: Rev 0 (USA)

## What the web port includes

- Emscripten/WASM runtime around the decompiled game logic
- WebGL2 rendering and browser event loop
- ROM picker flow for loading a local disc image or compatible dump
- IndexedDB-backed save mounting and persistence
- Touch controls for mobile-friendly play
- Post-processing filters such as CRT/LCD/halftone style effects
- Static output suitable for GitHub Pages hosting

## Requirements

- A modern browser with WebGL2 enabled
- A valid Animal Crossing GameCube disc image in a supported format
- For local builds: Docker or the Emscripten SDK

## Quick start with Docker

This repository includes a Docker setup that compiles the web client and serves the generated static bundle with Nginx on Alpine.

```bash
docker build -t slider-web .
docker run --rm -p 8080:80 slider-web
```

Then open:

```text
http://localhost:8080
```

The container serves the generated output from the static web bundle at the root of the site, matching the GitHub Pages layout.

## Local build for static hosting

To build the browser bundle locally:

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
```

The generated site is emitted under:

```text
build-web/web/
```

Those files are the exact static assets you would publish to GitHub Pages or copy into any web root for hosting.

## GitHub Pages / static hosting

The Emscripten target emits an `index.html` shell plus adjacent JS, wasm, CSS, and asset files so it can be served directly from a static root without a custom backend.

This is the same layout used by the project’s GitHub Pages workflow, and it is also what the included Nginx config serves in the Docker image.

## Controls

The browser build keeps the same input conventions as the game. The touch layer and keyboard bindings are handled in the web shell and mapped into the underlying SDL-style input path.

## Save data and ROM loading

The web client reads the ROM from the browser-side file picker and persists save data through browser storage. This avoids the need for a native filesystem layout and makes the app portable across static hosting environments.

## Credits

This project is built on the work of the [ACreTeam](https://github.com/ACreTeam) decompilation project. The web port extends that work into a browser-friendly build and static hosting workflow.

## FAQ

See [FAQ](FAQ.md) for more information.
