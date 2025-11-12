# Doxygen Extensions

This directory contains Doxygen extensions and the doxygen-awesome-css theme.

## Setup

The `doxygen-awesome-css` directory is a git submodule. To initialize it:

```bash
# From the repository root
git submodule update --init --recursive
```

Or if adding for the first time:

```bash
cd external/hf-max22200-driver/_config/doxygen-extensions
git submodule add https://github.com/jothepro/doxygen-awesome-css.git doxygen-awesome-css
```

## Usage

The Doxyfile should reference the doxygen-awesome-css files in this directory.
