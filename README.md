# HaxorEdit

Modern code editor for 1337 H4X0Rs.

## Building
### Dependencies
* CMake
* GLEW
* glfw
* glm
* freetype
* libgit2

Install them on Debian:
```
apt install cmake libglew-dev libglfw3-dev libglm-dev libfreetype-dev libgit2-dev
```

### Submodules
* [vscode-material-icon-theme](https://github.com/PKief/vscode-material-icon-theme) for icons
* [stb](https://github.com/nothings/stb) for image loading
* [cJSON](https://github.com/DaveGamble/cJSON) for JSON parsing
* [ICU](https://github.com/unicode-org/icu) for Unicode handling

### Compiling
```
python3 init.py
mkdir build
cd build
cmake ..
make
```
