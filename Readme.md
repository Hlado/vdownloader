![CI status](https://github.com/Hlado/vdownloader/actions/workflows/c-cpp.yml/badge.svg)

# Disclaimer

This is pet-project started for learning purposes, so don't expect any production quality or capabilities from it.

# Usage

***Vdownloader*** is an application to extract frames from local or remote (via http(s)) video files.
First video stream of container will be used.

Simple invocation may look like this:

    vdownloader <url/path> 0s-10s:9 20s500ms-30s500ms:9 ...

It will extract 11 frames (first & last frames + 9 inbetween)
from 0-10 seconds range and 11 frames from 20.5-30.5 seconds range
in tga format to current directory.

# Build

## Dependencies

- [CMake](https://cmake.org/) as a build system
- [vcpkg](https://vcpkg.io/en/) as a package manager
- [hyperfine](https://github.com/sharkdp/hyperfine) +
  [ffmpeg](https://www.ffmpeg.org/) +
  [python 3](https://www.python.org) +
  [yt-dlp](https://github.com/yt-dlp/yt-dlp) (optional, for benchmarking only)

## CMake invocation

Default setup for development currently is Windows 10 + VS Code / Visual Studio 2022.
Run following command to generate solution using CMake:

    cd <source_dir>
    cmake --preset=default
    
Project files will be placed into ***"build"*** subdirectory.