# Frequently Asked Questions

## What is the web port?

The web port is a browser-based build of the Animal Crossing decompilation. It compiles the game to WebAssembly with Emscripten and renders it through WebGL2, allowing the game to run inside a standard browser without a native desktop executable.

## Do I need a backend server?

No. The browser build is a static site. It runs as a WebAssembly app plus JS/CSS assets and stores save data in browser storage, typically IndexedDB. The app does not require a custom backend service to function.

## How do I load the game ROM in the browser?

The web shell exposes a ROM picker in the app. You select your legal copy of the Animal Crossing disc image and the browser-side runtime mounts it in the appropriate format for the game to load.

## Does the web build support saves?

Yes. Save data is persisted in the browser using IndexedDB, which is the same general model used by the project’s browser save mounting layer. This keeps the app portable while still preserving save state across refreshes and browser sessions.

## Why is the output designed for GitHub Pages and Nginx?

The Emscripten build emits a static site layout with an `index.html` shell and adjacent asset files. That lets the project be hosted from GitHub Pages or any generic web server. The included Docker/Nginx configuration is just a self-hosted version of the same static deployment pattern.

## Can I build the web client locally?

Yes. The project supports a local static build through Emscripten:

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
```

The generated files are under `build-web/web/` and can be served directly.

## Can I run it with Docker?

Yes. The repository includes a Dockerfile that compiles the web client and serves it via Nginx on Alpine:

```bash
docker build -t slider-web .
docker run --rm -p 8080:80 slider-web
```

## Does it work on mobile?

The browser build is intended to work on modern mobile browsers with WebGL2 support and touch input. Compatibility depends on the device/browser, but the web shell includes touch control support and mobile-friendly UI behaviors.

## Is this the same as the native PC port?

The web build shares the same game logic and asset handling, but the runtime is adapted for the browser environment. The native port is still a separate platform target, while the browser build is optimized for static hosting and in-browser execution.

## Is the web build legal to host publicly?

The project itself is a decompilation-based port, and the ROM data remains the user’s responsibility. The repository does not distribute the game assets. You must ensure that any ROM or save files you upload or load are from your own legal copy of the game.

## Why is the project not distributed as a standalone desktop binary here?

This repository is primarily focused on the browser port and static hosting workflow. The native desktop target still exists in the codebase, but the web-first deployment path is the main runtime being developed and documented here.