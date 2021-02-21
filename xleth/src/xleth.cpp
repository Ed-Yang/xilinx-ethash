#include <assert.h> /* assert */

#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>

#include <ethash/ethash.hpp>
#include <test/unittests/helpers.hpp>

using namespace std;
using namespace std::chrono;

//------------------------------------------------------------------------------
// ethash
//------------------------------------------------------------------------------

#define DIF_100M "0x0000002af31dc4611873bf3f70834acdae9f0f4f534f5d60585a5f1c1a3ced1b"
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
    bool noBinary = false;
    bool noExit = true; // note, noExit for AMD should be false, but here we set it true
    unsigned globalWorkSize = 0;
    unsigned globalWorkSizeMultiplier = 65536;
    // unsigned globalWorkSizeMultiplier = 4;
    unsigned localWorkSize = 128;
};

CLSettings m_settings;
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

void hex_dump(const char *hdr, uint8_t *data, size_t len)
{
    auto *ptr = data;

    printf("%s", hdr);

    for (int i = 0; i < len; i++, ptr++)
    {
        if (i > 0 && i % 64 == 0)
        {
            printf("\n");
        }
        printf("%02x", *ptr);
        if (i > 0 && (i%4) == 0)
            printf(" ");
    }
    std::cout << std::endl;
}

uint64_t get_target_value(ethash::hash256 &boundary)
{
    uint8_t tmp[8];
    uint64_t *pval = (uint64_t *)tmp;

    memcpy(tmp, boundary.bytes, 8);
    std::reverse(tmp, tmp + sizeof(tmp));
    const uint64_t target = *pval;

    return target;
}

class EthDevce
{
public:
    EthDevce() {}
    EthDevce(char *platform_name, char *knl_file, bool bBinary)
    {
        m_platform_name = platform_name;
        m_knl_file = knl_file;
        m_binary = bBinary; // binary kernel
    }

    vector<unsigned char> read_binary_file(const char *xclbin_file_name)
    {
        cout << "INFO: Reading " << xclbin_file_name << endl;
        FILE *fp;
        if ((fp = fopen(xclbin_file_name, "r")) == NULL)
        {
            printf("ERROR: %s xclbin not available please build\n",
                   xclbin_file_name);
            return vector<unsigned char>();
        }
        // Loading XCL Bin into char buffer
        cout << "Loading: '" << xclbin_file_name << "'\n";
        ifstream bin_file(xclbin_file_name, ifstream::binary);
        bin_file.seekg(0, bin_file.end);
        auto nb = bin_file.tellg();
        bin_file.seekg(0, bin_file.beg);
        vector<unsigned char> buf;
        buf.resize(nb);
        bin_file.read(reinterpret_cast<char *>(buf.data()), nb);
        return buf;
    }

    vector<cl::Device> get_devices(const string &platform_name)
    {
        size_t i;
        cl_int err;
        std::vector<cl::Platform> platforms;
        OCL_CHECK(err, err = cl::Platform::get(&platforms));
        cl::Platform platform;
        for (i = 0; i < platforms.size(); i++)
        {
            platform = platforms[i];
            OCL_CHECK(err, std::string platformName =
                               platform.getInfo<CL_PLATFORM_NAME>(&err));
            std::cout << "Got platform : " << platformName << std::endl;
            if (platformName.find(platform_name) != std::string::npos)
            {
                std::cout << "Found Platform" << std::endl;
                std::cout << "Platform Name: " << platformName.c_str() << std::endl;
                break;
            }
        }
        if (i == platforms.size())
        {
            std::cout << "Error: Failed to find platform: " << platform_name << std::endl;
            return vector<cl::Device>{};
        }
        // Getting ACCELERATOR Devices and selecting 1st such device
        std::vector<cl::Device> devices;

        if (!platform_name.compare("Xilinx"))
        {
            OCL_CHECK(err,
                      err = platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices));
        }
        else
        {
            OCL_CHECK(err,
                      err = platform.getDevices(CL_DEVICE_TYPE_GPU, &devices));
        }
        return devices;
    }

    void addDefinition(std::string &_source, char const *_id, unsigned _value)
    {
        char buf[256];
        sprintf(buf, "#define %s %uu\n", _id, _value);
        _source.insert(_source.begin(), buf, buf + strlen(buf));
    }

    bool load_kernel()
    {
        cl_int err;

        auto devices = get_devices(m_platform_name);

        if (!devices.size())
        {
            std::cout << "platform " << m_platform_name << " not found, kernel is not loaded" << std::endl;
            return false;
        }

        m_device = devices[0];

        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, m_context = cl::Context(m_device, NULL, NULL, NULL, &err));
        OCL_CHECK(err, m_queue = cl::CommandQueue(m_context, m_device,
                                                  CL_QUEUE_PROFILING_ENABLE, &err));

        std::cout << "Trying to program device " << m_device.getInfo<CL_DEVICE_NAME>() << std::endl;

        if (m_binary)
        {
            auto fileBuf = read_binary_file(m_knl_file);
            cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
            m_program = cl::Program(m_context, {m_device}, bins, NULL, &err);
        }
        else
        {
            std::ifstream t(m_knl_file);
            std::string code((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

            addDefinition(code, "WORKSIZE", m_settings.localWorkSize);
            addDefinition(code, "ACCESSES", 64);
            addDefinition(code, "MAX_OUTPUTS", c_maxSearchResults);
            addDefinition(code, "PLATFORM", 1); // 1:OPENCL_PLATFORM_AMD
            addDefinition(code, "FAST_EXIT", 1);

            cl::Program::Sources sources{{code.data(), code.size()}};
            m_program = cl::Program(m_context, sources);
            char options[256] = {0};
            m_program.build({m_device}, options);
        }

        if (err == CL_SUCCESS)
        {
            std::cout << "Device program successful.\n";
            OCL_CHECK(err, m_dagKernel = cl::Kernel(m_program, "GenerateDAG", &err));
            OCL_CHECK(err, m_searchKernel = cl::Kernel(m_program, "search", &err));
            m_knl_loaded = true;
        }
        else
        {
            std::cout << "Failed to program device with xclbin file!\n";
        }

        return m_knl_loaded;
    }

    bool get_context(int epoch)
    {
        ethash::epoch_context _ec = ethash::get_global_epoch_context(epoch);

        m_epochContext.epochNumber = epoch;
        m_epochContext.lightNumItems = _ec.light_cache_num_items;
        m_epochContext.lightSize = ethash::get_light_cache_size(_ec.light_cache_num_items);
        m_epochContext.dagNumItems = _ec.full_dataset_num_items;
        m_epochContext.dagSize = ethash::get_full_dataset_size(_ec.full_dataset_num_items);
        m_epochContext.lightCache = _ec.light_cache;

        return true;
    }

    void disp_device()
    {
        if (!m_knl_loaded)
        {
            return;
        }

        auto global_mem_size = m_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() / (1024 * 1024 * 1024);
        std::cout << "DEV: Global mem size  " << global_mem_size << " GB" << std::endl;

        auto max_alloc_size = m_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() / (1024 * 1024);
        std::cout << "DEV: Max alloc size   " << max_alloc_size << " MB" << std::endl;

        auto max_wg_size = m_device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
        std::cout << "DEV: Max W Group size " << max_wg_size << std::endl;

        auto max_c_uint = m_device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
        std::cout << "DEV: Max compute unit " << max_c_uint << std::endl;
    }

    bool gen_dag(int epoch)
    {
        cl_int err;

        if (!m_knl_loaded)
        {
            std::cout << "Kernel is not loaded !!!" << std::endl;
            return false;
        }

        std::cout << "DAG: generating for epoch " << epoch << " ..." << std::endl;
        get_context(epoch);

        m_dag.clear();
        if (m_epochContext.dagNumItems & 1)
        {
            m_dag.push_back(
                cl::Buffer(m_context, CL_MEM_READ_ONLY, m_epochContext.dagSize / 2 + 64));
            m_dag.push_back(
                cl::Buffer(m_context, CL_MEM_READ_ONLY, m_epochContext.dagSize / 2 - 64));
        }
        else
        {
            m_dag.push_back(
                cl::Buffer(m_context, CL_MEM_READ_ONLY, (m_epochContext.dagSize) / 2));
            m_dag.push_back(
                cl::Buffer(m_context, CL_MEM_READ_ONLY, (m_epochContext.dagSize) / 2));
        }

        m_light.clear();
        m_light.emplace_back(m_context, CL_MEM_READ_ONLY, m_epochContext.lightSize);

        //   OCL_CHECK(err, err = m_queue.enqueueMigrateMemObjects({m_lightCache},
        //                                                   0 /* 0 means from host*/));

        m_dagKernel.setArg(1, m_light[0]);

        OCL_CHECK(err, err = m_queue.enqueueWriteBuffer(m_light[0], CL_TRUE, 0,
                                                        m_epochContext.lightSize, m_epochContext.lightCache));

        // m_dagKernel.setArg(1, m_light[0]);
        m_dagKernel.setArg(2, m_dag[0]);
        m_dagKernel.setArg(3, m_dag[1]);
        m_dagKernel.setArg(4, (uint32_t)(m_epochContext.lightSize / 64));

        const uint32_t workItems = m_epochContext.dagNumItems * 2; // GPU computes partial 512-bit DAG items.

        uint32_t start;

        printf("DAG: epoch %u lightSize %lu dagSize %lu\n",
               epoch, m_epochContext.lightSize, m_epochContext.dagSize);

        const uint32_t chunk = 10000 * m_settings.localWorkSize;
        for (start = 0; start <= workItems - chunk; start += chunk)
        {
            auto t_start = high_resolution_clock::now();

            m_dagKernel.setArg(0, start);
            m_queue.enqueueNDRangeKernel(
                m_dagKernel, cl::NullRange, chunk, m_settings.localWorkSize);
            m_queue.finish();

            auto t_end = high_resolution_clock::now();

            auto duration = duration_cast<microseconds>(t_end - t_start);
            float ms = duration.count() / 1000;
            printf("DAG: item %10u chunk %u, took %6.2fs\n", start, chunk, ms/1000);
        }

        if (start < workItems)
        {
            uint32_t groupsLeft = workItems - start;
            groupsLeft = (groupsLeft + m_settings.localWorkSize - 1) / m_settings.localWorkSize;
            m_dagKernel.setArg(0, start);
            m_queue.enqueueNDRangeKernel(m_dagKernel, cl::NullRange,
                                         groupsLeft * m_settings.localWorkSize, m_settings.localWorkSize);
            m_queue.finish();
        }

        return true;
    }

    ethash::search_result search(int start_nonce, ethash::hash256 &seed, ethash::hash256 &header, ethash::hash256 &boundary)
    {
        volatile SearchResults results = {0};
        cl_int err;
        uint32_t zerox3[3] = {0, 0, 0};
        ethash::search_result r = {};

        if (!m_knl_loaded)
        {
            std::cout << "Kernel is not loaded !!!" << std::endl;
            return {};
        }

        const uint64_t target = get_target_value(boundary);
        printf("Search: target 0x%lx\n", target);

        // const uint64_t target = (uint64_t)(u64)((u256)boundary >> 192);  // edw. why ??
        assert(target > 0);

        uint64_t startNonce = start_nonce;

        m_header.clear();
        m_header.push_back(cl::Buffer(m_context, CL_MEM_READ_ONLY, 32));

        m_searchBuffer.clear();
        m_searchBuffer.emplace_back(m_context, CL_MEM_WRITE_ONLY, sizeof(SearchResults));

        m_searchKernel.setArg(0, m_searchBuffer[0]); // Supply output buffer to kernel.
        m_searchKernel.setArg(1, m_header[0]);       // Supply header buffer to kernel.

        // zero the result count
        OCL_CHECK(err, err = m_queue.enqueueWriteBuffer(
                           m_searchBuffer[0],
                           CL_FALSE,                       // blocking_write
                           offsetof(SearchResults, count), // offset
                           sizeof(zerox3),                 // size
                           zerox3));                       // data to be written
        // Update header constant buffer.

        OCL_CHECK(err, err = m_queue.enqueueWriteBuffer(
                           m_header[0], CL_FALSE, 0, 32, header.bytes));

        // m_searchKernel.setArg(0, m_searchBuffer[0]); // Supply output buffer to kernel.
        // m_searchKernel.setArg(1, m_header[0]);       // Supply header buffer to kernel.
        OCL_CHECK(err, m_searchKernel.setArg(2, m_dag[0])); // Supply DAG buffer to kernel.
        OCL_CHECK(err, m_searchKernel.setArg(3, m_dag[1])); // Supply DAG buffer to kernel.
        OCL_CHECK(err, m_searchKernel.setArg(4, m_epochContext.dagNumItems));
        OCL_CHECK(err, m_searchKernel.setArg(6, target));

        m_settings.globalWorkSize = m_settings.localWorkSize * m_settings.globalWorkSizeMultiplier;

        do
        {
            printf("Search: start nonce %lu, hash count %u\n", 
                startNonce, results.hashCount);

            OCL_CHECK(err, m_searchKernel.setArg(5, startNonce));
            OCL_CHECK(err, m_queue.enqueueNDRangeKernel(
                               m_searchKernel, cl::NullRange, m_settings.globalWorkSize,
                               m_settings.localWorkSize));

            OCL_CHECK(err, err = m_queue.enqueueReadBuffer(
                               m_searchBuffer[0],
                               CL_TRUE,
                               offsetof(SearchResults, count),
                               // (m_settings.noExit ? 1 : 2) * sizeof(results.count),
                                3 * sizeof(results.count), // read count, hashCount, abort
                               (void *)&results.count));

            if (results.count > 0)
            {
                OCL_CHECK(err, err = m_queue.enqueueReadBuffer(
                                   m_searchBuffer[0],
                                   CL_TRUE,
                                   0,                                       // offset
                                   results.count * sizeof(results.rslt[0]), // size
                                   (void *)&results));

                printf("Search: found, startNonce %lu, gid %u, hashCount %u abort %d\n",
                       startNonce, results.rslt[0].gid,
                       results.hashCount, results.abort);
                break;
            }
            startNonce += m_settings.globalWorkSize;

        } while (results.count == 0);

        if (results.count > 0)
        {
            r.solution_found = true;
            r.nonce = startNonce + results.rslt[0].gid;
            memcpy(r.mix_hash.bytes, (char *)results.rslt[0].mix, sizeof(results.rslt[0].mix));
        }

        return r;
    }

protected:
    char *m_platform_name;
    char *m_knl_file;
    bool m_binary; // binary kernel
    bool m_knl_loaded = false;

    // ethash
    struct EpochContext m_epochContext;

    // opencl
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
};

void usage(char **argv)
{
    // 0: xilinx, 1: amd
    std::cout << "Usage: " << std::endl;
    std::cout << argv[0] << " <epoch> <Xilinx|AMD> <kernel-file>" << std::endl;
}

int main(int argc, char **argv)
{
    int epoch;
    int start_nonce = 0;
    bool rv;

    if (argc < 4)
    {
        usage(argv);
        return EXIT_FAILURE;
    }

    epoch = atoi(argv[1]);

    EthDevce eth_dev;

    if (strcmp(argv[2], "Xilinx") == 0)
        eth_dev = EthDevce(argv[2], argv[3], true);
    else
        eth_dev = EthDevce(argv[2], argv[3], false);

    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Loading OpenCL kernel ..." << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    rv = eth_dev.load_kernel();
    if (!rv)
    {
        return 0;
    }
    eth_dev.disp_device();

    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Generating DAG ..." << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    auto t_start = high_resolution_clock::now();
    
    rv = eth_dev.gen_dag(epoch);

    auto t_end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t_end - t_start);
    float ms = duration.count() / 1000;
    printf("DAG: took %6.2f seconds.\n", ms/1000);

    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Searching ..." << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    // ethash::hash256 seed = ethash::calculate_epoch_seed(epoch);
    ethash::hash256 seed = {};
    ethash::hash256 header = {};
    std::string target_str = DIF_100M;
    ethash::hash256 boundary = to_hash256(target_str.substr(2));
    hex_dump("seed     : ", seed.bytes, 32);
    hex_dump("header   : ", header.bytes, 32);
    hex_dump("boundary : ", boundary.bytes, 32);

    ethash::search_result r = eth_dev.search(start_nonce, seed, header, boundary);

    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Check solution ..." << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    if (r.solution_found)
    {
        printf("Sol: nonce    : %lu\n", r.nonce);
        hex_dump("Sol: mix_hash : ", r.mix_hash.bytes, 32);

        // verify final
        ethash::epoch_context _ec = ethash::get_global_epoch_context(epoch);

        bool bValid = ethash::verify(_ec, header, r.mix_hash, r.nonce, boundary);
        printf("Sol: %s\n", (bValid) ? "valid." : "invalid !!!");
    }
    else
    {
        printf("do_search: no solution found !!!\n");
    }

    return 0;
}
