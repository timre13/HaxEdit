# HaxorEdit

Modern code editor for 1337 H4X0Rs.

## Building
### Dependencies
* CMake
* GLEW
* glfw
* glm
* freetype

Install them on Debian:
```
apt install cmake libglew-dev libglfw3-dev libglm-dev libfreetype-dev
```

### Submodules
* [vscode-material-icon-theme](https://github.com/PKief/vscode-material-icon-theme) for icons
* [stb](https://github.com/nothings/stb) for image loading

Run `git submodule init --recursive` to get the submodules.

### Compiling
```
mkdir build
cd build
cmake ..
make
```
