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
HaxorEdit uses the [vscode-material-icon-theme](vscode-material-icon-theme) project for icons.

Run `git submodule init --recursive` to get the submodule.

### Compiling
```
mkdir build
cd build
cmake ..
make
```
