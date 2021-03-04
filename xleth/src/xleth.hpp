#ifndef _XLETH_H
#define _XLETH_H

#include <assert.h> /* assert */

#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <atomic> 
#include <memory.h>

#include <ethash/ethash.hpp>
#include <test/unittests/helpers.hpp>

using namespace std;
using namespace std::chrono;

//------------------------------------------------------------------------------
// ethash
//------------------------------------------------------------------------------
// diff = '0x{:064x}'.format((int(2**256/DIFFICULTY)))
//DIFFICULTY = 100000000 
#define DIF_100M "0x0000002af31dc4611873bf3f70834acdae9f0f4f534f5d60585a5f1c1a3ced1b"
//DIFFICULTY = 200000000 
#define DIF_200M "0x00000015798ee2308c39df9fb841a566d74f87a7a9a7aeb02c2d2f8e0d1e768d"
//DIFFICULTY = 500000000 
#define DIF_500M "0x000000089705f4136b4a59731680a88f8953030fdd7645e011abac9f387295d2"

struct EpochContext
{
    // ethash::epoch_context m_ec;
    int epochNumber;
    int lightNumItems;
    size_t lightSize;
    const ethash_hash512 *lightCache;
    int dagNumItems;
    uint64_t dagSize;
};

struct CLSettings
{
    // inputs
    bool noBinary = false;
    bool noExit = true; // note, noExit for AMD should be false, but here we set it true
    unsigned localWorkSize = 128;
    unsigned globalWorkSizeMultiplier = 65536;
    // computed
    unsigned globalWorkSize = 0;
};

const size_t c_maxSearchResults = 4;

struct SearchResults
{
    struct
    {
        uint32_t gid;
        // Can't use h256 data type here since h256 contains
        // more than raw data. Kernel returns raw mix hash.
        uint32_t mix[8];
        uint32_t pad[7]; // pad to 16 words for easy indexing
    } rslt[c_maxSearchResults];
    uint32_t count;
    uint32_t hashCount;
    uint32_t abort;
};

//------------------------------------------------------------------------------
// OpenCL
//------------------------------------------------------------------------------
#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include <CL/cl2.hpp>

#define OCL_CHECK(error, call)                                                 \
    call;                                                                      \
    if (error != CL_SUCCESS)                                                   \
    {                                                                          \
        printf("%s:%d Error calling " #call ", error code is: %d\n", __FILE__, \
               __LINE__, error);                                               \
        exit(EXIT_FAILURE);                                                    \
    }

class EthDevce
{
protected:
    // opencl
    char *m_platform_name;
    char *m_knl_file;
    bool m_debug;
    vector<std::string> m_definitions;
    std::string m_code;
    bool m_binary; // binary kernel
    bool m_knl_loaded = false;
    CLSettings m_settings;

    cl::CommandQueue m_queue;
    cl::Context m_context;
    cl::Device m_device;
    cl::Program m_program;
    cl::Kernel m_dagKernel;
    cl::Kernel m_searchKernel;
    std::vector<cl::Buffer> m_dag;
    std::vector<cl::Buffer> m_light;
    std::vector<cl::Buffer> m_header;
    std::vector<cl::Buffer> m_searchBuffer;

    // hash rate
    std::chrono::steady_clock::time_point m_hashTime = std::chrono::steady_clock::now();
    // std::atomic<float> m_hashRate{0.0} ;
    float m_hashRate;
    uint64_t m_groupCount = 0;

    // ethash
    struct EpochContext m_epochContext;

public:
    EthDevce() {}
    EthDevce(char *platform_name, char *knl_file, bool bBinary, bool debug)
    {
        m_platform_name = platform_name;
        m_knl_file = knl_file;
        m_binary = bBinary; // binary kernel
        m_debug = debug;
    }
    bool set_params(unsigned localWorkSize, unsigned globalWorkSizeMultiplier, bool noExit) 
    {
        m_settings.localWorkSize = localWorkSize;
        m_settings.globalWorkSizeMultiplier = globalWorkSizeMultiplier;
        m_settings.noExit = noExit;
    }
    vector<unsigned char> read_binary_file(const char *xclbin_file_name);
    vector<cl::Device> get_devices(const string &platform_name);
    void add_definition(char const *_id, unsigned _value);
    bool load_kernel();
    bool get_context(int epoch);
    void disp_device();
    bool gen_dag(int epoch);
    ethash::search_result search(int start_nonce, ethash::hash256 &seed, ethash::hash256 &header, ethash::hash256 &boundary);
    bool verify(ethash::hash256 &header, ethash::hash256& mix_hash, uint64_t nonce, ethash::hash256 &boundary)
    {
        ethash::epoch_context _ec = ethash::get_global_epoch_context(m_epochContext.epochNumber);
        return ethash::verify(_ec, header, mix_hash, nonce, boundary);
    }
    float get_hashrate() noexcept;
    void update_hashrate(uint32_t _groupSize, uint32_t _increment) noexcept;
};

void hex_dump(const char *hdr, uint8_t *data, size_t len);

#endif // _XLETH_H