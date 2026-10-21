// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions
#include <PiCanTest/Top/PiCanTestTopology.hpp>
// OSAL initialization
#include <Os/Os.hpp>
// Used for signal handling shutdown
#include <signal.h>
// Used for command line argument processing
#include <getopt.h>
// Used for atoi
#include <cstdlib>
// Used for logging to the console
#include <Fw/Logger/Logger.hpp>

/**
 * \brief print command line help message
 *
 * This will print a command line help message including the available command line arguments.
 *
 * @param app: name of application
 */
void print_usage(const char* app) {
    Fw::Logger::log(
        "Usage: ./%s [options]\n"
        "-a\taddress the GDS TCP server listens on (default 0.0.0.0)\n"
        "-p\tport the GDS TCP server listens on (default 50000)\n"
        "-i\tSocketCAN interface (default can0)\n"
        "-e\tanswer pings from startup\n",
        app);
}

/**
 * \brief shutdown topology cycling on signal
 *
 * The reference topology allows for a simulated cycling of the rate groups. This simulated cycling needs to be stopped
 * in order for the program to shutdown. This is done via handling signals such that it is performed via Ctrl-C
 *
 * @param signum
 */
static void signalHandler(int signum) {
    PiCanTest::stopRateGroups();
}

/**
 * \brief execute the program
 *
 * This F´ program is designed to run in standard environments (e.g. Linux/macOs running on a laptop). Thus it uses
 * command line inputs to specify how to connect.
 *
 * @param argc: argument count supplied to program
 * @param argv: argument values supplied to program
 * @return: 0 on success, something else on failure
 */
int main(int argc, char* argv[]) {
    I32 option = 0;
    const CHAR* hostname = "0.0.0.0";
    U16 port_number = 50000;
    const CHAR* canInterface = "can0";
    bool echo = false;

    Os::init();

    // Loop while reading the getopt supplied options
    while ((option = getopt(argc, argv, "hep:a:i:")) != -1) {
        switch (option) {
            // Handle the -a argument for address/hostname
            case 'a':
                hostname = optarg;
                break;
            // Handle the -p port number argument
            case 'p':
                port_number = static_cast<U16>(atoi(optarg));
                break;
            case 'i':
                canInterface = optarg;
                break;
            case 'e':
                echo = true;
                break;
            // Cascade intended: help output
            case 'h':
            // Cascade intended: help output
            case '?':
            // Default case: output help and exit
            default:
                print_usage(argv[0]);
                return (option == 'h') ? 0 : 1;
        }
    }
    // Object for communicating state to the topology
    PiCanTest::TopologyState inputs;
    inputs.hostname = hostname;
    inputs.port = port_number;
    inputs.canInterface = canInterface;
    inputs.echo = echo;

    // Setup program shutdown via Ctrl-C
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    Fw::Logger::log("Hit Ctrl-C to quit\n");

    // Setup, cycle, and teardown topology
    PiCanTest::setupTopology(inputs);
    PiCanTest::startRateGroups();
    PiCanTest::teardownTopology(inputs);
    Fw::Logger::log("Exiting...\n");
    return 0;
}
