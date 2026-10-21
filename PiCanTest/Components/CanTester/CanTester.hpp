// ======================================================================
// \title  CanTester.hpp
// \brief  hpp file for CanTester component implementation class
// ======================================================================

#ifndef PiCanTest_CanTester_HPP
#define PiCanTest_CanTester_HPP

#include <atomic>

#include "PiCanTest/Components/CanTester/CanTesterComponentAc.hpp"
#include "PiCanTest/Components/CanTester/FppConstantsAc.hpp"
#include "SocketCan/Ports/FppConstantsAc.hpp"

namespace PiCanTest {

class CanTester final : public CanTesterComponentBase {
  public:
    //! Longest run of TX_BUSY results a burst waits through before giving up
    static constexpr U32 BURST_MAX_BUSY_RETRIES = 1000;

    //! Pause between retries when the TX queue is full, in microseconds
    static constexpr U32 BURST_RETRY_DELAY_US = 200;

    explicit CanTester(const char* const compName);

    ~CanTester() override = default;

    //! Set whether pings are answered. Call before the component's task starts;
    //! afterward use the SET_ECHO command.
    void setEcho(bool enabled) { this->m_echo = enabled; }

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    void canReceive_handler(FwIndexType portNum, const SocketCan::CanFrame& frame, const Fw::Time& rxTime) override;

    //! Runs on the driver's read task when our queue is full
    void canReceive_overflowHook(FwIndexType portNum,
                                 const SocketCan::CanFrame& frame,
                                 const Fw::Time& rxTime) override;

    //! Sends the next ping if a ping run is active, then publishes telemetry
    void run_handler(FwIndexType portNum, U32 context) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    void SEND_FRAME_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 frameId, bool extended, U8 len, U64 payload) override;
    void SET_ECHO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool enabled) override;
    void START_PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 count) override;
    void STOP_PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void SEND_BURST_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 count, bool extended) override;
    void RESET_COUNTERS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Send one frame, counting and reporting failures
    SocketCan::CanStatus sendFrame(U32 frameId, bool extended, U8 len, const U8* data);

    void sendPing();
    void handlePong(const SocketCan::CanFrame& frame, U32 nowUs);
    void handleBurstFrame(const SocketCan::CanFrame& frame);
    void finishPingRun();

    //! Current time in microseconds, truncated to 32 bits (wraps every ~71 minutes)
    U32 nowUs();

    static void putU32(U8* out, U32 value);
    static U32 getU32(const U8* in);

    // ----------------------------------------------------------------------
    // State (component thread only, except where atomic)
    // ----------------------------------------------------------------------

    bool m_echo;
    bool m_pinging;
    U32 m_pingTarget;  //!< Pings to send in this run; 0 means until stopped

    U32 m_pingSeq;
    U32 m_pingsSent;
    U32 m_pongsReceived;
    U32 m_lastRttUs;
    U32 m_minRttUs;
    U32 m_maxRttUs;
    U32 m_echoesSent;

    U32 m_burstSeq;  //!< Next sequence number to send
    U32 m_burstTx;
    U32 m_burstRx;
    U32 m_burstLost;
    U32 m_nextBurstSeq;  //!< Sequence number expected next
    bool m_burstTracking;

    U32 m_otherRx;
    U32 m_sendFailures;

    std::atomic<U32> m_queueOverflows;  //!< Written on the read task
};

}  // namespace PiCanTest

#endif
