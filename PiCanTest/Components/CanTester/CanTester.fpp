module PiCanTest {

    @ Frame ID the pinging node sends; an echoing node answers with PONG_ID
    constant CAN_TEST_PING_ID = 0x100

    @ Frame ID an echoing node answers a ping with
    constant CAN_TEST_PONG_ID = 0x101

    @ Frame ID of burst frames; data bytes 0-3 are a big-endian sequence number
    constant CAN_TEST_BURST_ID = 0x200

    @ Exercises SocketCan.LinuxCanDriver against another node on the bus.
    @ Run the same deployment on both nodes: one pings or bursts, the other echoes.
    active component CanTester {

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Send one frame with the given ID and up to 8 payload bytes
        @ (payload is big-endian, left-aligned: the first byte is the top byte)
        async command SEND_FRAME(
            frameId: U32 @< CAN identifier
            extended: bool @< Use a 29-bit identifier
            len: U8 @< Data length, 0-8
            payload: U64 @< Payload bytes, first byte in the most significant position
        )

        @ Answer pings (frame ID 0x100) with a pong (frame ID 0x101)
        async command SET_ECHO(enabled: bool)

        @ Send one ping per second, count times (0 means until STOP_PING)
        async command START_PING($count: U32)

        @ Stop pinging
        async command STOP_PING()

        @ Send count back-to-back burst frames (frame ID 0x200) with sequence numbers.
        @ A receiving node reports sequence gaps. Stops early if the TX queue stays full.
        async command SEND_BURST($count: U32, extended: bool)

        @ Zero the test counters and the burst sequence tracking
        async command RESET_COUNTERS()

        # ----------------------------------------------------------------------
        # Ports
        # ----------------------------------------------------------------------

        @ Frames to transmit
        output port canSend: SocketCan.CanSend

        @ Frames from the bus. Called on the driver's read task, so it queues.
        async input port canReceive: SocketCan.CanReceive hook

        @ Rate group input: drives the ping timer and publishes telemetry
        async input port run: Svc.Sched drop

        time get port timeCaller

        import Fw.Command
        import Fw.Event
        import Fw.Channel

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ A frame we did not recognize as part of the test protocol
        event FrameReceived(
            frameId: U32 @< CAN identifier
            extended: bool @< 29-bit identifier
            len: U8 @< Data length
        ) \
            severity activity low \
            format "Rx frame id 0x{x} ext {} len {}" \
            throttle 20

        @ A ping was answered
        event PongReceived(
            seq: U32 @< Ping sequence number
            rttUs: U32 @< Round trip time in microseconds
        ) \
            severity activity high \
            format "Pong {} rtt {} us"

        @ Frames were missing from a burst
        event BurstGap(
            expected: U32 @< Sequence number we expected
            got: U32 @< Sequence number that arrived
        ) \
            severity warning high \
            format "Burst gap: expected {} got {}" \
            throttle 10

        @ A transmit did not succeed
        event SendFailed(
            status: SocketCan.CanStatus @< The driver's result
        ) \
            severity warning low \
            format "CAN send failed: {}" \
            throttle 10

        @ A send command was rejected because the payload length is invalid
        event InvalidLength(
            len: U8 @< The length requested
        ) \
            severity warning low \
            format "Invalid CAN data length {}"

        @ A burst finished sending
        event BurstDone(
            sent: U32 @< Frames accepted by the driver
            requested: U32 @< Frames requested
            txBusy: U32 @< Times the TX queue was full
        ) \
            severity activity high \
            format "Burst sent {} of {} frames, TX queue full {} times"

        @ A finite ping run finished
        event PingDone(
            sent: U32 @< Pings sent
            received: U32 @< Pongs received
        ) \
            severity activity high \
            format "Ping run done: {} sent, {} pongs"

        @ The canReceive queue overflowed and frames were dropped
        event ReceiveQueueOverflow \
            severity warning high \
            format "CanTester receive queue overflowed" \
            throttle 5

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Pings sent
        telemetry PingsSent: U32

        @ Pongs received
        telemetry PongsReceived: U32

        @ Most recent ping round trip, microseconds
        telemetry LastRttUs: U32

        @ Smallest ping round trip since reset, microseconds
        telemetry MinRttUs: U32

        @ Largest ping round trip since reset, microseconds
        telemetry MaxRttUs: U32

        @ Pings answered while echo was enabled
        telemetry EchoesSent: U32

        @ Burst frames accepted by the driver
        telemetry BurstTx: U32

        @ Burst frames received
        telemetry BurstRx: U32

        @ Burst frames missing, from sequence gaps
        telemetry BurstLost: U32

        @ Other data frames received
        telemetry OtherRx: U32

        @ Sends that did not return OK
        telemetry SendFailures: U32

        @ Frames dropped because the receive queue was full
        telemetry QueueOverflows: U32

    }

}
