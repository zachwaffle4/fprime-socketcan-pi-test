// ======================================================================
// \title  PiCanTestTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <PiCanTest/Top/PiCanTestTopologyAc.hpp>
// Note: Uncomment when using Svc:TlmPacketizer
//#include <PiCanTest/Top/PiCanTestPacketsAc.hpp>

// Necessary project-specified types
#include <Fw/Logger/Logger.hpp>
#include <Fw/Types/MallocAllocator.hpp>
#include <Fw/Types/String.hpp>

// Public functions for use in main program are namespaced with deployment module PiCanTest
// This is also the namespace where the topology components are instantiated by FPP.
namespace PiCanTest {

// Instantiate a malloc allocator for cmdSeq buffer allocation
Fw::MallocAllocator mallocator;

// Rate group timing: base clock interval and divisors are coupled to rate group names
const Fw::TimeInterval rateGroupInterval(1, 0);  // 1Hz base clock
Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{1, 0}, {2, 0}, {4, 0}}};
// Divisors: 1Hz, 0.5Hz, 0.25Hz

// Context tokens for rate group members (unused, set to zero)
Svc::ActiveRateGroup::ContextArray rateGroup_1HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_5HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_25HzContext(0);

enum TopologyConstants {
    COMM_PRIORITY = 34,
    //! Requested SO_RCVBUF for the CAN socket; the kernel caps it at net.core.rmem_max
    CAN_RECEIVE_BUFFER_BYTES = 256 * 1024,
};

// True once the CAN read task is running, so teardown knows to stop it
static bool canStarted = false;

/**
 * \brief configure/setup components in project-specific way
 *
 * This is a *helper* function which configures/sets up each component requiring project specific input. This includes
 * allocating resources, passing-in arguments, etc. This function may be inlined into the topology setup function if
 * desired, but is extracted here for clarity.
 */
void configureTopology() {
    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisorsSet);

    // Rate groups require context arrays.
    rateGroup_1Hz.configure(rateGroup_1HzContext);
    rateGroup_0_5Hz.configure(rateGroup_0_5HzContext);
    rateGroup_0_25Hz.configure(rateGroup_0_25HzContext);

    // Command sequencer needs to allocate memory to hold contents of command sequences
    cmdSeq.allocateBuffer(0, mallocator, 5 * 1024);

    // PrmDb file name must be supplied by the using topology
    FileHandling::prmDb.configure("PrmDb.dat");
}

/**
 * \brief open the CAN interface and start the driver's read task
 *
 * The interface must already be configured and up (see scripts/setup-can.sh). On failure the driver has emitted
 * an InterfaceOpenFailed or InterfaceDown event; the deployment keeps running so that event can be read in the GDS.
 */
static void startCan(const TopologyState& state) {
    const Fw::String interfaceName(state.canInterface);
    if (can.open(interfaceName, CAN_RECEIVE_BUFFER_BYTES)) {
        can.start();
        canStarted = true;
    } else {
        Fw::Logger::log("Could not open CAN interface %s; is it up? CAN is disabled\n", state.canInterface);
    }
}

void setupTopology(const TopologyState& state) {
    // Autocoded initialization. Function provided by autocoder.
    initComponents(state);
    // Autocoded id setup. Function provided by autocoder.
    setBaseIds();
    // Autocoded connection wiring. Function provided by autocoder.
    connectComponents();
    // Autocoded command registration. Function provided by autocoder.
    regCommands();
    // Autocoded configuration. Function provided by autocoder.
    configComponents(state);
    if (state.hostname != nullptr && state.port != 0) {
        comDriver.configure(state.hostname, state.port);
    }
    // Project-specific component configuration. Function provided above. May be inlined, if desired.
    configureTopology();
    canTester.setEcho(state.echo);
    // Autocoded parameter read from file. Function provided by autocoder.
    readParameters();
    // Autocoded parameter loading. Function provided by autocoder.
    loadParameters();
    // Autocoded task kick-off (active components). Function provided by autocoder.
    startTasks(state);
    startCan(state);
    // Initialize socket communication if and only if there is a valid specification
    if (state.hostname != nullptr && state.port != 0) {
        Os::TaskString name("ReceiveTask");
        // Uplink is configured for receive so a socket task is started
        comDriver.start(name, COMM_PRIORITY, Default::STACK_SIZE);
    }
}

void startRateGroups() {
    // Blocks until stopRateGroups() is called (e.g. from signal handler)
    timer.startTimer(rateGroupInterval);
}

void stopRateGroups() {
    timer.quit();
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);

    // Other task clean-up.
    if (canStarted) {
        can.quitReadThread();
        (void)can.join();
        canStarted = false;
    }
    comDriver.stop();
    (void)comDriver.join();

    // Resource deallocation
    cmdSeq.deallocateBuffer(mallocator);

    tearDownComponents(state);
    deinitComponents(state);
}
};  // namespace PiCanTest
