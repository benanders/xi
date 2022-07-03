# Xi

Xi is a simple CLI text editor written in pure C, for fun.

Features include:
* Syntax highlighting
* Multiple cursors
* Autocompletion and autoindentation
* Tabs
* Split views
* Mouse support

My goals for this project are:
* **Easy to use**: simple to install and use, with common default keybindings.
* **Standalone**: xi should compile to a single executable without any dependent system libraries or headers.
* **Customisable**: lots of options and configurable keybindings in JSON format for easy editing.
* **Maintainable**: clearly written, well documented, easily maintainable source code.

### Building

You can compile Xi using [CMake](https://cmake.org/) with:

```bash
$ git clone https://github.com/benanders/xi
$ cd xi
$ mkdir cmake-build-debug
$ cd cmake-build-debug
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ cmake --build ..
$ ./xi
```
