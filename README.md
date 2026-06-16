# FASL Compression

FASL is an experimental near-optimal lossless compression algorithm designed to work with a wide variety of data types. The project is currently under active development and is far from complete, but you are welcome to explore, test, and contribute to its development.

## Building the Project

To compile the project, simply run:

```bash
make
```

The build system is primarily designed for Linux and has been tested on Linux-based systems. It may also work on macOS and Windows environments, although this has not been thoroughly tested.

After a successful build, two executables will be generated:

* `compress` – Compresses an input file.
* `decompress` – Restores a compressed file to its original form.

## Usage

### Compressing a File

```bash
./compress <input_file> <output_file>
```

Example:

```bash
./compress tests/graph_theory.wiki out.fasl
```

This command compresses `graph_theory.wiki` and stores the compressed output in `out.fasl`.

### Decompressing a File

```bash
./decompress <input_file> <output_file>
```

Example:

```bash
./decompress out.fasl outputs/graph_theory.wiki
```

This command decompresses `out.fasl` and recreates the original file at `outputs/graph_theory.wiki`.

## Project Status

FASL is currently a research project and work in progress. The compression algorithm, file format, and performance characteristics may change significantly as development continues. Feedback, testing, and contributions are welcome.

