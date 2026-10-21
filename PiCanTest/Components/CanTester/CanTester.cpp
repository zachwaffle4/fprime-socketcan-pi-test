// ======================================================================
// \title  CanTester.cpp
// \brief  cpp file for CanTester component implementation class
// ======================================================================

#include "PiCanTest/Components/CanTester/CanTester.hpp"

#include <Fw/Types/Assert.hpp>
#include <Os/Task.hpp>
#include <cstring>

namespace PiCanTest {

CanTester ::CanTester(const char* const compName)
    : CanTesterComponentBase(compName),
      m_echo(false),
      m_pinging(false),
      m_pingTarget(0),
      m_pingSeq(0),
      m_pingsSent(0),
      m_pongsReceived(0),
      m_lastRttUs(0),
      m_minRttUs(0),
      m_maxRttUs(0),
      m_echoesSent(0),
      m_burstSeq(0),
      m_burstTx(0),
      m_burstRx(0),
      m_burstLost(0),
      m_nextBurstSeq(0),
      m_burstTracking(false),
      m_otherRx(0),
      m_sendFailures(0),
      m_queueOverflows(0) {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void CanTester ::canReceive_handler(const FwIndexType portNum,
                                    const SocketCan::CanFrame& frame,
                                    const Fw::Time& rxTime) {
    const bool extended = (frame.get_flags() & SocketCan::CAN_FRAME_FLAG_IDE) != 0;
    const U32 frameId = frame.get_id();

    // Only bursts use 29-bit IDs in the test protocol; ping and pong are 11-bit
    if (extended && frameId != CAN_TEST_BURST_ID) {
        this->m_otherRx++;
        this->log_ACTIVITY_LO_FrameReceived(frameId, true, frame.get_len());
        return;
    }

    switch (frameId) {
        case CAN_TEST_PING_ID:
            if (this->m_echo && frame.get_len() == 8) {
                // Echo the payload back unchanged so the sender can compute the round trip
                if (this->sendFrame(CAN_TEST_PONG_ID, false, 8, frame.get_data()) == SocketCan::CanStatus::OK) {
                    this->m_echoesSent++;
                }
            } else {
                this->m_otherRx++;
            }
            break;
        case CAN_TEST_PONG_ID:
            if (frame.get_len() == 8) {
                this->handlePong(frame, this->nowUs());
            } else {
                this->m_otherRx++;
            }
            break;
        case CAN_TEST_BURST_ID:
            if (frame.get_len() >= 4) {
                this->handleBurstFrame(frame);
            } else {
                this->m_otherRx++;
            }
            break;
        default:
            this->m_otherRx++;
            this->log_ACTIVITY_LO_FrameReceived(frameId, false, frame.get_len());
            break;
    }
}

void CanTester ::canReceive_overflowHook(const FwIndexType portNum,
                                         const SocketCan::CanFrame& frame,
                                         const Fw::Time& rxTime) {
    this->m_queueOverflows++;
}

void CanTester ::run_handler(const FwIndexType portNum, const U32 context) {
    if (this->m_pinging) {
        if (this->m_pingTarget != 0 && this->m_pingsSent >= this->m_pingTarget) {
            this->finishPingRun();
        } else {
            this->sendPing();
        }
    }

    this->tlmWrite_PingsSent(this->m_pingsSent);
    this->tlmWrite_PongsReceived(this->m_pongsReceived);
    this->tlmWrite_LastRttUs(this->m_lastRttUs);
    this->tlmWrite_MinRttUs(this->m_minRttUs);
    this->tlmWrite_MaxRttUs(this->m_maxRttUs);
    this->tlmWrite_EchoesSent(this->m_echoesSent);
    this->tlmWrite_BurstTx(this->m_burstTx);
    this->tlmWrite_BurstRx(this->m_burstRx);
    this->tlmWrite_BurstLost(this->m_burstLost);
    this->tlmWrite_OtherRx(this->m_otherRx);
    this->tlmWrite_SendFailures(this->m_sendFailures);

    const U32 overflows = this->m_queueOverflows;
    this->tlmWrite_QueueOverflows(overflows);
    if (overflows != 0) {
        this->log_WARNING_HI_ReceiveQueueOverflow();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void CanTester ::SEND_FRAME_cmdHandler(const FwOpcodeType opCode,
                                       const U32 cmdSeq,
                                       const U32 frameId,
                                       const bool extended,
                                       const U8 len,
                                       const U64 payload) {
    if (len > SocketCan::CAN_FRAME_MAX_LEN) {
        this->log_WARNING_LO_InvalidLength(len);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    U8 data[SocketCan::CAN_FRAME_MAX_LEN];
    for (U8 i = 0; i < SocketCan::CAN_FRAME_MAX_LEN; i++) {
        data[i] = static_cast<U8>(payload >> (8 * (SocketCan::CAN_FRAME_MAX_LEN - 1 - i)));
    }

    const SocketCan::CanStatus status = this->sendFrame(frameId, extended, len, data);
    this->cmdResponse_out(opCode, cmdSeq,
                          (status == SocketCan::CanStatus::OK) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void CanTester ::SET_ECHO_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq, const bool enabled) {
    this->m_echo = enabled;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CanTester ::START_PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq, const U32 count) {
    this->m_pingTarget = this->m_pingsSent + count;
    if (count == 0) {
        this->m_pingTarget = 0;
    }
    this->m_pinging = true;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CanTester ::STOP_PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
    if (this->m_pinging) {
        this->finishPingRun();
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CanTester ::SEND_BURST_cmdHandler(const FwOpcodeType opCode,
                                       const U32 cmdSeq,
                                       const U32 count,
                                       const bool extended) {
    U32 sent = 0;
    U32 txBusy = 0;
    U32 consecutiveBusy = 0;

    while (sent < count && consecutiveBusy < BURST_MAX_BUSY_RETRIES) {
        U8 data[SocketCan::CAN_FRAME_MAX_LEN] = {};
        putU32(data, this->m_burstSeq);

        // Not sendFrame(): a full TX queue is expected here and is counted
        // separately from failures
        const SocketCan::CanFrame frame(CAN_TEST_BURST_ID, extended ? SocketCan::CAN_FRAME_FLAG_IDE : 0, 4, data);
        const SocketCan::CanStatus status = this->canSend_out(0, frame);

        if (status == SocketCan::CanStatus::OK) {
            sent++;
            this->m_burstSeq++;
            this->m_burstTx++;
            consecutiveBusy = 0;
        } else if (status == SocketCan::CanStatus::TX_BUSY) {
            txBusy++;
            consecutiveBusy++;
            (void)Os::Task::delay(Fw::TimeInterval(0, BURST_RETRY_DELAY_US));
        } else {
            this->m_sendFailures++;
            this->log_WARNING_LO_SendFailed(status);
            break;
        }
    }

    this->log_ACTIVITY_HI_BurstDone(sent, count, txBusy);
    this->cmdResponse_out(opCode, cmdSeq, (sent == count) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void CanTester ::RESET_COUNTERS_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
    this->m_pingSeq = 0;
    this->m_pingsSent = 0;
    this->m_pongsReceived = 0;
    this->m_lastRttUs = 0;
    this->m_minRttUs = 0;
    this->m_maxRttUs = 0;
    this->m_echoesSent = 0;
    this->m_burstSeq = 0;
    this->m_burstTx = 0;
    this->m_burstRx = 0;
    this->m_burstLost = 0;
    this->m_nextBurstSeq = 0;
    this->m_burstTracking = false;
    this->m_otherRx = 0;
    this->m_sendFailures = 0;
    this->m_queueOverflows = 0;
    this->m_pinging = false;
    this->m_pingTarget = 0;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

SocketCan::CanStatus CanTester ::sendFrame(const U32 frameId, const bool extended, const U8 len, const U8* const data) {
    U8 padded[SocketCan::CAN_FRAME_MAX_LEN] = {};
    FW_ASSERT(len <= SocketCan::CAN_FRAME_MAX_LEN, len);
    (void)std::memcpy(padded, data, len);

    const SocketCan::CanFrame frame(frameId, extended ? SocketCan::CAN_FRAME_FLAG_IDE : 0, len, padded);
    const SocketCan::CanStatus status = this->canSend_out(0, frame);
    if (status != SocketCan::CanStatus::OK) {
        this->m_sendFailures++;
        this->log_WARNING_LO_SendFailed(status);
    }
    return status;
}

void CanTester ::sendPing() {
    U8 data[8];
    putU32(&data[0], this->m_pingSeq);
    putU32(&data[4], this->nowUs());
    if (this->sendFrame(CAN_TEST_PING_ID, false, 8, data) == SocketCan::CanStatus::OK) {
        this->m_pingSeq++;
        this->m_pingsSent++;
    }
}

void CanTester ::handlePong(const SocketCan::CanFrame& frame, const U32 nowUs) {
    const U8* const data = frame.get_data();
    const U32 seq = getU32(&data[0]);
    // Unsigned subtraction stays correct across a wrap of the 32-bit microsecond clock
    const U32 rttUs = nowUs - getU32(&data[4]);

    this->m_pongsReceived++;
    this->m_lastRttUs = rttUs;
    if (this->m_pongsReceived == 1 || rttUs < this->m_minRttUs) {
        this->m_minRttUs = rttUs;
    }
    if (rttUs > this->m_maxRttUs) {
        this->m_maxRttUs = rttUs;
    }
    this->log_ACTIVITY_HI_PongReceived(seq, rttUs);
}

void CanTester ::handleBurstFrame(const SocketCan::CanFrame& frame) {
    const U32 seq = getU32(frame.get_data());
    this->m_burstRx++;

    if (!this->m_burstTracking) {
        this->m_burstTracking = true;
    } else if (seq != this->m_nextBurstSeq) {
        // A sequence below the expected one means the sender restarted, not a loss
        if (seq > this->m_nextBurstSeq) {
            this->m_burstLost += seq - this->m_nextBurstSeq;
            this->log_WARNING_HI_BurstGap(this->m_nextBurstSeq, seq);
        }
    }
    this->m_nextBurstSeq = seq + 1;
}

void CanTester ::finishPingRun() {
    this->m_pinging = false;
    this->log_ACTIVITY_HI_PingDone(this->m_pingsSent, this->m_pongsReceived);
}

U32 CanTester ::nowUs() {
    const Fw::Time now = this->getTime();
    return static_cast<U32>(static_cast<U64>(now.getSeconds()) * 1000000ULL + now.getUSeconds());
}

void CanTester ::putU32(U8* const out, const U32 value) {
    out[0] = static_cast<U8>(value >> 24);
    out[1] = static_cast<U8>(value >> 16);
    out[2] = static_cast<U8>(value >> 8);
    out[3] = static_cast<U8>(value);
}

U32 CanTester ::getU32(const U8* const in) {
    return (static_cast<U32>(in[0]) << 24) | (static_cast<U32>(in[1]) << 16) | (static_cast<U32>(in[2]) << 8) |
           static_cast<U32>(in[3]);
}

}  // namespace PiCanTest
