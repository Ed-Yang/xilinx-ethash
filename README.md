# Xilinx Ethash Test

This project is created for testing the Ethminer's ethash opencl kernel on Xilinx Alveo U50 PCIe card.

* Testing Environment

    ```shell
    CPU: Intel(R) Core(TM) i5-4460  CPU @ 3.20GHz
    RAN: 16GB
    OS: Ubuntu 18.04.5 LTS
    Xilinx: Alveo U50/Vitis 2020.2
    ```

* Alveo U50 information:

    ```shell
    $ sudo /opt/xilinx/xrt/bin/xbmgmt flash --scan
    ```

    ```shell
    Card [0000:01:00.0]
        Card type:		u50
        Flash type:		SPI
        Flashable partition running on FPGA:
            xilinx_u50_gen3x16_xdma_201920_3,[ID=0xf465b0a3ae8c64f6],[SC=5.0]
        Flashable partitions installed in system:	
            xilinx_u50_gen3x16_xdma_201920_3,[ID=0xf465b0a3ae8c64f6],[SC=5.1.7]
    ```

* Source tree:

    ```shell

    xilinx-ethash
    ├── CMakeLists.txt
    ├── README.md
    └── xleth
        ├── config
        │   ├── connectivity_u50.ini <-- U50 HBM settings
        │   └── xrt.ini
        ├── kernel
        │   ├── ethash-01112021.cl <-- new ethminer opencl kernel (Xilinx build failed)
        │   ├── ethash-08222020.cl <-- original ethminer opencl kernel (base)
        │   └── ethash.cl <-- from ethminer with minor modification
        ├── src
        │   ├── xleth.cpp <-- opencl application
        └── xclbin
            ├── ethash.hw.xclbin <-- binary stream used for U50
            └── ethash.sw_emu.xclbin  <-- binary stream used for software emulation

    ```

    Clone source:

    ```shell
    git clone --recursive https://github.com/Ed-Yang/xilinx-ethash.git
    ```

## Questions

* How comes running in Alveo U50 is slower than software emulation ?


    Summary:

    | Cards	DAG generation | (epoch=0) | 
    | --- | --- | 
    | Xilinx Alveo U50 sw_emu	| 588.30 seconds |
    | Xilinx Alveo U50 PCIe	| 1709.76 seconds |
    | AMD RX560	| 6.00 seconds |

<br>

* Does Xilinx's OpenCL support atomic_inc or alternative function ?

## Build Xilinx binary stream

* Setup build environment

    Open a terminal and run:

    ```shell
    source /tools/Xilinx/Vitis/2020.2/settings64.sh
    source /opt/xilinx/xrt/setup.sh
    export XILINX_VITIS=/tools/Xilinx/Vitis/2020.2
    ```

    ```shell
    export PATH=$PATH:/opt/xilinx/xrt/bin
    export XILINX_XRT=/opt/xilinx/xrt
    ```

* Build binary stream for software emulation 

    ```shell
    cd ./xleth
    make build TARGET=sw_emu DEVICE=xilinx_u50_gen3x16_xdma_201920_3
    ```

* Build binary stream for U50

    ```shell
    cd ./xleth
    make build TARGET=hw DEVICE=xilinx_u50_gen3x16_xdma_201920_3
    ```

    Output binary stream will be copied to xclbin directory

## Packages

    OpenCL:

    ```shell
    sudo apt install -y ocl-icd-opencl-dev opencl-headers gdb
    ```

    CMake

    ```shell
    # upgrade openssl
    wget wget https://www.openssl.org/source/openssl-1.1.1j.tar.gz
    tar xvf openssl-1.1.1j.tar.gz 
    cd openssl-1.1.1j/
    ./config
    make
    make install

    # upgrade CMake (need new openssl)
    wget https://github.com/Kitware/CMake/releases/download/v3.19.5/cmake-3.19.5.tar.gz
    tar xvf cmake-3.19.5.tar.gz
    cd cmake-3.19.5
    ./bootstrap
    make
    sudo make install
    ```

## Build host applicatopm

### Build ethash library

Open a terminal with CMake version > 3.13.

    ```shell
    mkdir -p ./ethash/build
    cd ./ethash/build
    cmake ..
    make
    ```

### Build application

    ```shell
    mkdir -p ./build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug
    make
    ```

## Run application - Xilinx

* Setup environment

    ```shell
    source /tools/Xilinx/Vitis/2020.2/settings64.sh
    source /opt/xilinx/xrt/setup.sh
    export XILINX_VITIS=/tools/Xilinx/Vitis/2020.2
    ```

    Usage: 
    ./build/xleth <epoch> <Xilinx|AMD> <kernel-file>

### Run software emulation

* Setup emulation mode

    ```shell
    export XCL_EMULATION_MODE=sw_emu
    ```

* Generate emconfig.json

    The file "emconfig.json" must be located at the same directory as executable "xleth".

    ```shell
    cd ./build
    /tools/Xilinx/Vitis/2020.2/bin/emconfigutil -f xilinx_u50_gen3x16_xdma_201920_3
    ```

    Run:

    ```shell
    ./build/xleth 0 Xilinx ./xleth/xclbin/ethash.sw_emu.xclbin 
    ```

    Output:

    ```shell
    -----------------------------------------------
    Loading OpenCL kernel ...
    -----------------------------------------------
    Got platform : Xilinx
    Found Platform
    Platform Name: Xilinx
    Trying to program device xilinx_u50_gen3x16_xdma_201920_3
    INFO: Reading ./xleth/xclbin/ethash.sw_emu.xclbin
    Loading: './xleth/xclbin/ethash.sw_emu.xclbin'
    Device program successful.
    DEV: Global mem size  8 GB
    DEV: Max alloc size   4096 MB
    DEV: Max W Group size 4294967295
    DEV: Max compute unit 2
    -----------------------------------------------
    Generating DAG ...
    -----------------------------------------------
    DAG: generating for epoch 0 ...
    DAG: epoch 0 lightSize 16776896 dagSize 1073739904
    DAG: item          0 chunk 1280000, took  43.83s
    DAG: item    1280000 chunk 1280000, took  44.76s
    DAG: item    2560000 chunk 1280000, took  44.80s
    DAG: item    3840000 chunk 1280000, took  45.03s
    DAG: item    5120000 chunk 1280000, took  43.59s
    DAG: item    6400000 chunk 1280000, took  44.01s
    DAG: item    7680000 chunk 1280000, took  45.99s
    DAG: item    8960000 chunk 1280000, took  44.74s
    DAG: item   10240000 chunk 1280000, took  43.54s
    DAG: item   11520000 chunk 1280000, took  44.33s
    DAG: item   12800000 chunk 1280000, took  46.63s
    DAG: item   14080000 chunk 1280000, took  45.99s
    DAG: item   15360000 chunk 1280000, took  45.53s
    DAG: took 588.30 seconds.
    -----------------------------------------------
    Searching ...
    -----------------------------------------------
    seed     : 0000000000 00000000 00000000 00000000 00000000 00000000 00000000 000000
    header   : 0000000000 00000000 00000000 00000000 00000000 00000000 00000000 000000
    boundary : 0000002af3 1dc46118 73bf3f70 834acdae 9f0f4f53 4f5d6058 5a5f1c1a 3ced1b
    Search: target 0x2af31dc461
    Search: start nonce 0, hash count 0
    Search: found, startNonce 0, gid 3664863, hashCount 0 abort 0
    -----------------------------------------------
    Check solution ...
    -----------------------------------------------
    Sol: nonce    : 3664863
    Sol: mix_hash : 09f1a1567c a8f6fbbf 6e8c6793 a6474814 7e0d9998 e2e21821 2bb05bff 99360a
    Sol: valid.
    ```

### Run on Alveo U50

    Run:

    ```shell
    ./build/xleth 0 Xilinx ./xleth/xclbin/ethash.hw.xclbin
    ```

    Output:

    ```shell
    -----------------------------------------------
    Loading OpenCL kernel ...
    -----------------------------------------------
    Got platform : Xilinx
    Found Platform
    Platform Name: Xilinx
    Trying to program device xilinx_u50_gen3x16_xdma_201920_3
    INFO: Reading ./xleth/xclbin/ethash.hw.xclbin
    Loading: './xleth/xclbin/ethash.hw.xclbin'
    Device program successful.
    DEV: Global mem size  0 GB
    DEV: Max alloc size   4096 MB
    DEV: Max W Group size 4294967295
    DEV: Max compute unit 2
    -----------------------------------------------
    Generating DAG ...
    -----------------------------------------------
    DAG: generating for epoch 0 ...
    DAG: epoch 0 lightSize 16776896 dagSize 1073739904
    DAG: item          0 chunk 1280000, took 130.49s
    DAG: item    1280000 chunk 1280000, took 130.38s
    DAG: item    2560000 chunk 1280000, took 130.39s
    DAG: item    3840000 chunk 1280000, took 130.40s
    DAG: item    5120000 chunk 1280000, took 130.40s
    DAG: item    6400000 chunk 1280000, took 130.38s
    DAG: item    7680000 chunk 1280000, took 130.35s
    DAG: item    8960000 chunk 1280000, took 130.37s
    DAG: item   10240000 chunk 1280000, took 130.38s
    DAG: item   11520000 chunk 1280000, took 130.36s
    DAG: item   12800000 chunk 1280000, took 130.39s
    DAG: item   14080000 chunk 1280000, took 130.39s
    DAG: item   15360000 chunk 1280000, took 130.41s
    DAG: took 1709.76 seconds.
    -----------------------------------------------
    Searching ...
    -----------------------------------------------
    seed     : 0000000000 00000000 00000000 00000000 00000000 00000000 00000000 000000
    header   : 0000000000 00000000 00000000 00000000 00000000 00000000 00000000 000000
    boundary : 0000002af3 1dc46118 73bf3f70 834acdae 9f0f4f53 4f5d6058 5a5f1c1a 3ced1b
    Search: target 0x2af31dc461
    Search: start nonce 0, hash count 0
    Search: found, startNonce 0, gid 3664863, hashCount 0 abort 0
    -----------------------------------------------
    Check solution ...
    -----------------------------------------------
    Sol: nonce    : 3664863
    Sol: mix_hash : 09f1a1567c a8f6fbbf 6e8c6793 a6474814 7e0d9998 e2e21821 2bb05bff 99360a
    Sol: valid.
    ```

### Run on AMD RX560

    Run:

    ```shell
    ./build/xleth 0 AMD ./xleth/kernel/ethash.cl
    ```

    Output:

    ```shell
    -----------------------------------------------
    Loading OpenCL kernel ...
    -----------------------------------------------
    Got platform : Xilinx
    Got platform : AMD Accelerated Parallel Processing
    Found Platform
    Platform Name: AMD Accelerated Parallel Processing
    Trying to program device Baffin
    Device program successful.
    DEV: Global mem size  3 GB
    DEV: Max alloc size   3260 MB
    DEV: Max W Group size 256
    DEV: Max compute unit 14
    -----------------------------------------------
    Generating DAG ...
    -----------------------------------------------
    DAG: generating for epoch 0 ...
    DAG: epoch 0 lightSize 16776896 dagSize 1073739904
    DAG: item          0 chunk 1280000, took   0.69s
    DAG: item    1280000 chunk 1280000, took   0.40s
    DAG: item    2560000 chunk 1280000, took   0.40s
    DAG: item    3840000 chunk 1280000, took   0.40s
    DAG: item    5120000 chunk 1280000, took   0.40s
    DAG: item    6400000 chunk 1280000, took   0.40s
    DAG: item    7680000 chunk 1280000, took   0.40s
    DAG: item    8960000 chunk 1280000, took   0.40s
    DAG: item   10240000 chunk 1280000, took   0.40s
    DAG: item   11520000 chunk 1280000, took   0.40s
    DAG: item   12800000 chunk 1280000, took   0.40s
    DAG: item   14080000 chunk 1280000, took   0.40s
    DAG: item   15360000 chunk 1280000, took   0.40s
    DAG: took   6.00 seconds.
    -----------------------------------------------
    Searching ...
    -----------------------------------------------
    seed     : 0000000000 00000000 00000000 00000000 00000000 00000000 00000000 000000
    header   : 0000000000 00000000 00000000 00000000 00000000 00000000 00000000 000000
    boundary : 0000002af3 1dc46118 73bf3f70 834acdae 9f0f4f53 4f5d6058 5a5f1c1a 3ced1b
    Search: target 0x2af31dc461
    Search: start nonce 0, hash count 0
    Search: found, startNonce 0, gid 3664863, hashCount 28687 abort 1
    -----------------------------------------------
    Check solution ...
    -----------------------------------------------
    Sol: nonce    : 3664863
    Sol: mix_hash : 09f1a1567c a8f6fbbf 6e8c6793 a6474814 7e0d9998 e2e21821 2bb05bff 99360a
    Sol: valid.
    ```

## Reference

Xilinx

* [Alveo U50 Getting Started](https://www.xilinx.com/products/boards-and-kits/alveo/u50.html#gettingStarted)
* [Vitis Accel Examples' Repository](https://github.com/Xilinx/Vitis_Accel_Examples)
* [Xilinx: Profiling the Application](https://www.xilinx.com/html_docs/xilinx2020_2/vitis_doc/profilingapplication.html#vfc1586356138757)

Ethash

* [Ethash GitHub](https://github.com/chfast/ethash)
* [Ethminer GitHub](https://github.com/ethereum-mining/ethminer)





