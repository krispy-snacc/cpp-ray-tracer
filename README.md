# 🌟 C++ Ray Tracer

Welcome to **C++ Ray Tracer** – a modern, multi-threaded ray tracing engine written in C++. This project is designed as a learning tool: experiment with materials, build your own scenes, and discover the fundamentals of computer graphics, one pixel at a time!

---

## Features

- **Physically-based rendering**: Realistic lighting, reflections, and refractions
- **Multi-threaded**: Fast rendering using all your CPU cores
- **Modular design**: Easy to extend with new objects and materials
- **Gamma-corrected output**: Images look great on any display
- **PNG export**: High-quality output via [stb_image_write.h](include/stb_image_write.h)

---

## Quick Start

### Build

```sh
mkdir build
cd build
cmake ..
make
```

### Run

```sh
./bin/MyRayTracer
```

Your rendered image will be saved as `image.png`.

---

## Project Structure

```
include/   # Header files (Vec3, Color, Scene, etc.)
src/       # Source code
bin/       # Compiled binaries
build/     # Build artifacts
```

---

## How It Works

- Rays are cast from a virtual camera into a 3D scene.
- Objects (like spheres) are intersected, and materials determine their appearance.
- Lighting, shadows, and reflections are computed recursively.
- The final image is written to a PNG file.

---

## Credits

- [stb_image_write.h](https://github.com/nothings/stb) by Sean Barrett
- Inspired by Peter Shirley’s "Ray Tracing in One Weekend" series
