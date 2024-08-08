# Building this project

## Required build tools

1. [Meson](https://mesonbuild.com/)
1. [Ninja](https://ninja-build.org/)

## Fetch submodule dependencies

From the root of the project, run:

```bash
git submodule update --init --recursive
```

## Configure and build

```bash
# Release
meson setup --buildtype=release ./build

# Debug
meson setup --buildtype=debug ./build
```

```bash
ninja -C ./build
```

## Build and run project test suite

```bash
cd ./tests
meson setup --buildtype=debug ./build
ninja -C ./build
./build/test_suite --gtest_brief=1
```
