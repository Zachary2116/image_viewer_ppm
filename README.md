# PPM Image Viewer

A lightweight image viewer for PPM files written in C using SDL3.

## Features

- Supports both P3 (ASCII) and P6 (binary) PPM formats
- Scroll to zoom in/out
- Resizable window with aspect-correct scaling

## Requirements

- GCC
- SDL3 (included in `lib/` and `include/`)

## Build

```bash
make
```

## Usage

```bash
./image_viewer <file.ppm>
```

## Controls

| Action | Control |
|--------|---------|
| Zoom in/out | Scroll wheel |
| Quit | Close window |
