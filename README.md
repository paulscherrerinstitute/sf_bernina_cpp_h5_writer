# sf_cpp_h5_writer
H5 detector data writer for SwissFEL.

# Table of content
1. [Quick start](#quick_start)
2. [Build](#build)
    1. [Conda setup](#conda_setup)
    2. [Local build](#local_build)
3. [REST interface](#rest_interface)
4. [File format](#file_format)


<a id="quick_start"></a>
# Quick start

```bash
Usage: sf_cpp_h5_writer [connection_address] [output_file] [n_frames] [rest_port] [user_id]
        connection_address: Address to connect to the stream (PULL). Example: tcp://127.0.0.1:40000
        output_file: Name of the output file.
        n_frames: Number of images to acquire. 0 for infinity (untill /stop is called).
        rest_port: Port to start the REST Api on.
        user_id: uid under which to run the writer. -1 to leave it as it is.
```

After the writer process has started, you can communicate with it using the REST interface.

For more information see the library documentation [lib\_cpp\_h5\_writer](https://github.com/paulscherrerinstitute/sf_cpp_h5_writer).

## Shared library problems
Sometimes some shared libraries are missing from your library path (this is deployment dependent). In this case, if you are using a conda 
environment as suggested, you can execute this:

```bash
export LD_LIBRARY_PATH=${CONDA_PREFIX}/lib:${LD_LIBRARY_PATH}
```

If you are not using conda environemnts, you will have to deal with it yourself.

<a id="build"></a>
# Build

The easiest way to build the library is via Anaconda. If you are not familiar with Anaconda (and do not want to learn), 
please see the [Local build](#local_build) chapter.

<a id="conda_setup"></a>
## Conda setup
If you use conda, you can create an environment with the sf_cpp_h5_writer library by running:

```bash
conda create -c paulscherrerinstitute --name <env_name> sf_cpp_h5_writer
```

After that you can just source you newly created environment and run the writer executable.
In case you cannot start the program because of missing libraries, the most common cure is to update your LD\_LIBRARY\_PATH
```bash
export LD_LIBRARY_PATH=${CONDA_PREFIX}/lib:${LD_LIBRARY_PATH}
```

<a id="local_build"></a>
## Local build
You can build the library by running make in the root folder of the project:

```bash
make clean all
```

or by using the conda also from the root folder of the project:

```bash
conda build conda-recipe
```

Both methods require you to have a sourced conda environament with the below specified requirements installed.

### Requirements
The library relies on the following packages:

- make
- gcc
- lib\_cpp\_h5\_writer

When you are using conda to install the packages, you might need to add the **conda-forge** and **paulscherrerinstitute** channels to
your conda config:

```
conda config --add channels conda-forge
conda config --add channels paulscherrerinstitute
```

<a id="rest_interface"></a>
# REST interface
Details about the rest interface can be found in the writer library documentation: 
[lib\_cpp\_h5\_writer](https://github.com/paulscherrerinstitute/sf_cpp_h5_writer#rest_interface)

<a id="file_format"></a>
# File format