#include "xleth.hpp"

void usage(char **argv)
{
    // 0: xilinx, 1: amd
    std::cout << "Usage: " << std::endl;
    std::cout << argv[0] << " <epoch> <Xilinx|AMD> <kernel-file> [quiet]" << std::endl;
}

int main(int argc, char **argv)
{
    int epoch;
    bool rv;
    bool debug = true;

    if (argc < 4)
    {
        usage(argv);
        return EXIT_FAILURE;
    }

    if (argc > 4)
        debug = false;

    epoch = atoi(argv[1]);

    EthDevce eth_dev;

    if (strcmp(argv[2], "Xilinx") == 0)
    {
        eth_dev = EthDevce(argv[2], argv[3], true, debug);
    }
    else
    {
        eth_dev = EthDevce(argv[2], argv[3], false, debug);

        unsigned localWorkSize = 128;
        unsigned globalWorkSizeMultiplier = 65535;
        bool no_exit = true;

        eth_dev.set_params(localWorkSize, globalWorkSizeMultiplier, false);
    }

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
    printf("DAG: took %6.2f seconds.\n", ms / 1000);

    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Searching ..." << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    // ethash::hash256 seed = ethash::calculate_epoch_seed(epoch);
    ethash::hash256 seed = {};
    ethash::hash256 header = {};
    std::string target_str = DIF_200M;
    ethash::hash256 boundary = to_hash256(target_str.substr(2));
    hex_dump("seed     : ", seed.bytes, 32);
    hex_dump("header   : ", header.bytes, 32);
    hex_dump("boundary : ", boundary.bytes, 32);

    int start_nonce = 0;
    ethash::search_result r = eth_dev.search(start_nonce, seed, header, boundary);

    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Check solution ..." << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    if (r.solution_found)
    {
        printf("Sol: nonce    : %lu\n", r.nonce);
        hex_dump("Sol: mix_hash : ", r.mix_hash.bytes, 32);

        bool bValid = eth_dev.verify(header, r.mix_hash, r.nonce, boundary);
        printf("Sol: %s\n", (bValid) ? "valid." : "invalid !!!");

        printf("Sol: Hash rate %5.2f Mh \n", eth_dev.get_hashrate());
    }
    else
    {
        printf("do_search: no solution found !!!\n");
    }

    return 0;
}
