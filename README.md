# Simple OpenAI Image Client

A small Qt 6 desktop app for generating and editing images with OpenAI's image models
by describing what you want in plain language. Builds on Linux and Windows.

## Providers

- **OpenAI** (default): uses your OpenAI API key and models such as `gpt-image-1`.
- **OpenRouter**: uses an OpenRouter key and OpenAI's image models as OpenRouter lists
  them, such as `openai/gpt-image-1`.

Set the provider, API key and model in **File → Settings**. If the key field is empty,
the app falls back to the `OPENAI_API_KEY` or `OPENROUTER_API_KEY` environment variable.
Keys entered in Settings are stored in plain text in the app's settings
(`~/.config/SimpleImageClient/` on Linux, the registry on Windows).

## Generating images

Type what you want in the prompt box, pick a size, quality and how many images (1–4),
then press **Generate** (or Ctrl+Enter). Generation can take a minute or more; the
button turns into **Cancel** while it runs. Results appear in the preview, with
thumbnails when there are several; **Copy** puts the shown image on the clipboard and
**Save…** writes it as PNG or JPEG.

With OpenRouter, size is sent as an aspect ratio and the image count may be ignored by
the model, since OpenRouter serves image models through its chat API.

## Building

Requires CMake 3.21+, a C++17 compiler and Qt 6.4 or newer (Core, Gui, Widgets, Network).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
./build/SimpleImageClient
```

On Ubuntu/Debian: `sudo apt install qt6-base-dev cmake g++`. On Windows, install Qt with
the Qt Online Installer (MSVC 2022 64-bit) and configure from a Visual Studio developer
prompt, passing `-DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64`.

Every push builds on Linux and Windows in GitHub Actions; the Windows job uploads a
ready-to-run folder (with Qt DLLs) as a build artifact.
