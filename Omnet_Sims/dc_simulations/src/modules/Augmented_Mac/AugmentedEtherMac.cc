/*
 * Copyright (C) 2003 Andras Varga; CTIE, Monash University, Australia
 * Copyright (C) 2011 Zoltan Bojthe
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>


#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/ethernet/EtherEncap.h"
#include "inet/linklayer/ethernet/EtherFrame_m.h"
#include "./AugmentedEtherMac.h"
#include "inet/linklayer/ethernet/EtherPhyFrame_m.h"
#include "inet/linklayer/ethernet/Ethernet.h"
#include "inet/networklayer/common/InterfaceEntry.h"

using namespace inet;

#define V2PIFOPRIO 0
#define V2PIFOCANARY 1

// TODO: there is some code that is pretty much the same as the one found in EtherMacFullDuplex.cc (e.g. EtherMac::beginSendFrames)
// TODO: refactor using a statemachine that is present in a single function
// TODO: this helps understanding what interactions are there and how they affect the state

static std::ostream& operator<<(std::ostream& out, cMessage *msg)
{
    out << "(" << msg->getClassName() << ")" << msg->getFullName();
    return out;
}

Define_Module(AugmentedEtherMac);

simsignal_t AugmentedEtherMac::collisionSignal = registerSignal("collision");
simsignal_t AugmentedEtherMac::backoffSlotsGeneratedSignal = registerSignal("backoffSlotsGenerated");
simsignal_t AugmentedEtherMac::pfcGeneratedSignal = registerSignal("pfcGeneratedSignal");

AugmentedEtherMac::~AugmentedEtherMac()
{
    delete frameBeingReceived;
    cancelAndDelete(endRxMsg);
    cancelAndDelete(endBackoffMsg);
    cancelAndDelete(endJammingMsg);

    if (base_signal_packet)
        delete base_signal_packet;
    recordScalar("lightFinalTransmitState", transmitState);

}

void AugmentedEtherMac::initialize(int stage)
{
    EtherMacBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        endRxMsg = new cMessage("EndReception", ENDRECEPTION);
        endBackoffMsg = new cMessage("EndBackoff", ENDBACKOFF);
        endJammingMsg = new cMessage("EndJamming", ENDJAMMING);

        port_idx = getParentModule()->getIndex();

        // initialize state info
        backoffs = 0;
        numConcurrentTransmissions = 0;
        currentSendPkTreeID = 0;

        WATCH(backoffs);
        WATCH(numConcurrentTransmissions);

        use_v2_pifo = getAncestorPar("use_v2_pifo");
        use_pfabric = getAncestorPar("use_pfabric");
        use_vertigo_prio_queue = getAncestorPar("use_vertigo_prio_queue");
        send_header_of_dropped_packet_to_receiver = getAncestorPar("send_header_of_dropped_packet_to_receiver");
        switch_name = getParentModule()->getParentModule()->getFullName();

        // bolt
        use_bolt_queue = getAncestorPar("use_bolt_queue");
        use_bolt_with_vertigo_queue = getAncestorPar("use_bolt_with_vertigo_queue");
        use_bolt = use_bolt_queue || use_bolt_with_vertigo_queue;

        // assign the buffer module, if there is no buffer module, we should popout error
        std::string buffer_name = getAncestorPar("bufferModule");
        std::string buffer_full_name = buffer_name;
        int buffer_idx = getParentModule()->getParentModule()->getIndex();
        buffer_full_name += "[" + std::to_string(buffer_idx) + "]";
        cModule* buffer_module = getModuleByPath(buffer_full_name.c_str());
        if (buffer_module == nullptr)
            buffer = nullptr;
        else {
            buffer = check_and_cast<V2PacketBuffer*>(buffer_module);
        }

        if (use_vertigo_prio_queue && !use_bolt) {
            std::string v2pifo_queue_type_str = getAncestorPar("v2pifo_queue_type");
            if (v2pifo_queue_type_str.compare("prio") == 0)
                v2pifo_queue_type = V2PIFOPRIO;
            else if (v2pifo_queue_type_str.compare("canary") == 0)
                v2pifo_queue_type = V2PIFOCANARY;
            else
                throw cRuntimeError("No relative priority calculation paradigm for canary!");
        }

        use_pfc = getAncestorPar("use_pfc");
    }
}

void AugmentedEtherMac::initializeStatistics()
{
    EtherMacBase::initializeStatistics();

    framesSentInBurst = 0;
    bytesSentInBurst = B(0);

    WATCH(framesSentInBurst);
    WATCH(bytesSentInBurst);

    // initialize statistics
    totalCollisionTime = 0.0;
    totalSuccessfulRxTxTime = 0.0;
    numCollisions = numBackoffs = 0;

    WATCH(numCollisions);
    WATCH(numBackoffs);
}

void AugmentedEtherMac::initializeFlags()
{
    EtherMacBase::initializeFlags();

    duplexMode = par("duplexMode");
    frameBursting = !duplexMode && par("frameBursting");
    physInGate->setDeliverOnReceptionStart(true);
}

void AugmentedEtherMac::processConnectDisconnect()
{
    if (!connected) {
        delete frameBeingReceived;
        frameBeingReceived = nullptr;
        cancelEvent(endRxMsg);
        cancelEvent(endBackoffMsg);
        cancelEvent(endJammingMsg);
        bytesSentInBurst = B(0);
        framesSentInBurst = 0;
    }

    EtherMacBase::processConnectDisconnect();

    if (connected) {
        if (!duplexMode) {
            // start RX_RECONNECT_STATE
            changeReceptionState(RX_RECONNECT_STATE);
            simtime_t reconnectEndTime = simTime() + b(MAX_ETHERNET_FRAME_BYTES + JAM_SIGNAL_BYTES).get() / curEtherDescr->txrate;
            endRxTimeList.clear();
            addReceptionInReconnectState(-1, reconnectEndTime);
        }
    }
}

void AugmentedEtherMac::readChannelParameters(bool errorWhenAsymmetric)
{
    EtherMacBase::readChannelParameters(errorWhenAsymmetric);

    if (connected && !duplexMode) {
        if (curEtherDescr->halfDuplexFrameMinBytes < B(0))
            throw cRuntimeError("%g bps Ethernet only supports full-duplex links", curEtherDescr->txrate);
    }
}

void AugmentedEtherMac::handleSelfMessage(cMessage *msg)
{
    // Process different self-messages (timer signals)
    EV_TRACE << "Self-message " << msg << " received\n";

    if (!bw_set && use_bolt) {
        bw_set = true;
        std::string queue_path = getFullPath() + ".queue";
        if (use_bolt_queue) {
            V2PIFOBoltQueue * queue_module = check_and_cast<V2PIFOBoltQueue *>(getModuleByPath(queue_path.c_str()));
            queue_module->set_BW_bps(transmissionChannel->getNominalDatarate());
        } else if (use_bolt_with_vertigo_queue) {
            V2PIFO * queue_module = check_and_cast<V2PIFO *>(getModuleByPath(queue_path.c_str()));
            queue_module->set_BW_bps(transmissionChannel->getNominalDatarate());
        }
    }

    switch (msg->getKind()) {
        case ENDIFG:
            handleEndIFGPeriod();
            break;

        case ENDTRANSMISSION:
            handleEndTxPeriod();
            break;

        case ENDRECEPTION:
            handleEndRxPeriod();
            break;

        case ENDBACKOFF:
            handleEndBackoffPeriod();
            break;

        case ENDJAMMING:
            handleEndJammingPeriod();
            break;

        case ENDPAUSE:
            handleEndPausePeriod();
            break;

        default:
            throw cRuntimeError("Self-message with unexpected message kind %d", msg->getKind());
    }
}

void AugmentedEtherMac::handleMessageWhenUp(cMessage *msg)
{
    if (channelsDiffer)
        readChannelParameters(true);

    printState();

    // some consistency check
    if (!duplexMode && transmitState == TRANSMITTING_STATE && receiveState != RX_IDLE_STATE)
        throw cRuntimeError("Inconsistent state -- transmitting and receiving at the same time");

    if (msg->isSelfMessage())
        handleSelfMessage(msg);
    else if (msg->getArrivalGateId() == upperLayerInGateId)
        handleUpperPacket(check_and_cast<Packet *>(msg));
    else if (msg->getArrivalGate() == physInGate) {
        if (auto jamSignal = dynamic_cast<EthernetJamSignal *>(msg))
            processJamSignalFromNetwork(jamSignal);
        else
            processMsgFromNetwork(check_and_cast<EthernetSignal *>(msg));
    }
    else
        throw cRuntimeError("Message received from unknown gate");

    processAtHandleMessageFinished();
    printState();
}

void AugmentedEtherMac::check_on_the_way_variables() {
    if (on_the_way_packet_length < b(0) || on_the_way_packet_num < 0) {
        std::cout << "on_the_way_packet_length: " << on_the_way_packet_length << ", on_the_way_packet_num: "
                << on_the_way_packet_num << endl;
        throw cRuntimeError("on_the_way_packet_length < b(0) || on_the_way_packet_num < 0");
    }

    if (buffer != nullptr && (buffer->on_the_way_packet_length < b(0) ||
            buffer->on_the_way_packet_num < 0))
        throw cRuntimeError("buffer != nullptr && (buffer->on_the_way_packet_length < b(0) || "
                "buffer->on_the_way_packet_num < 0)");
}

void AugmentedEtherMac::handleUpperPacket(Packet *packet)
{

    EV_INFO << "Received " << packet << " from upper layer." << endl;

    if (!use_pfc && (txQueue->pfc_status != txQueue->PFC_RESUME || txQueue->neighbor_pfc_status != txQueue->PFC_RESUME))
        throw cRuntimeError("handleUpperPacket: !use_pfc && (txQueue->pfc_status != txQueue->PFC_RESUME || txQueue->neighbor_pfc_status != txQueue->PFC_RESUME)");

    numFramesFromHL++;
    emit(packetReceivedFromUpperSignal, packet);

    MacAddress address = getMacAddress();

    auto frame = packet->peekAtFront<EthernetMacHeader>();
    if (frame->getDest().equals(address)) {
        throw cRuntimeError("Logic error: frame %s from higher layer has local MAC address as dest (%s)",
                packet->getFullName(), frame->getDest().str().c_str());
    }

    if (packet->getDataLength() > MAX_ETHERNET_FRAME_BYTES) {
        throw cRuntimeError("Packet length from higher layer (%s) exceeds maximum Ethernet frame size (%s)",
                packet->getDataLength().str().c_str(), MAX_ETHERNET_FRAME_BYTES.str().c_str());
    }

    if (!connected || disabled) {
        EV_WARN << (!connected ? "Interface is not connected" : "MAC is disabled") << " -- dropping packet " << frame << endl;
        PacketDropDetails details;
        details.setReason(INTERFACE_DOWN);
        emit(packetDroppedSignal, packet, &details);
        numDroppedPkFromHLIfaceDown++;
        delete packet;

        return;
    }
    // fill in src address if not set
    if (frame->getSrc().isUnspecified()) {
        EV << "frame->getSrc() is Unspecified" << endl;
        //frame is immutable
        frame = nullptr;
        auto newFrame = packet->removeAtFront<EthernetMacHeader>();
        newFrame->setSrc(address);
        packet->insertAtFront(newFrame);
        frame = newFrame;
    }

    addPaddingAndSetFcs(packet, MIN_ETHERNET_FRAME_BYTES);  // calculate valid FCS

    // store frame and possibly begin transmitting
    EV_DETAIL << "Frame " << packet << " arrived from higher layer, enqueueing\n";
    if (!frame->getIs_v2_dropped_packet_header()) {
//        std::string switch_name = getParentModule()->getParentModule()->getFullName();
        std::string module_path_string = switch_name + ".eth[" +
                std::to_string(getParentModule()->getIndex()) + "].mac";
        std::string queue_full_path = module_path_string + ".queue";
        if (send_header_of_dropped_packet_to_receiver) {
            queue_full_path = module_path_string + ".queue.mainQueue";
//            std::cout << getFullPath() << endl;
//            std::cout << queue_full_path << endl;
//            std::cout << "----------------------------------" << endl;
        }

        // TODO: YOU SHOULD REMOVE THIS ERROR FOR RED
        if (!use_vertigo_prio_queue && !use_v2_pifo && !use_pfabric) {
            bool queue_full = is_queue_full(b(packet->getBitLength()), queue_full_path);
            if (queue_full)
                throw cRuntimeError("The queue is full but you're still pushing while you should've dropped in relay unit.");
        }
        on_the_way_packet_length -= b(packet->getBitLength());
//        EV << getFullPath() << ", removing packet length is: " << b(packet->getBitLength()) << endl;
//        std::cout << packet->str() << endl;

        on_the_way_packet_num -= 1;
        if (buffer != nullptr) {
            buffer->on_the_way_packet_length -= b(packet->getBitLength());
            buffer->on_the_way_packet_num -= 1;
        }
    }
    check_on_the_way_variables();
    txQueue->pushPacket(packet);

    if (!currentTxFrame && !txQueue->isEmpty())
        popTxQueue();

    if ((duplexMode || receiveState == RX_IDLE_STATE) && transmitState == TX_IDLE_STATE) {
        EV_DETAIL << "No incoming carrier signals detected, frame clear to send\n";
        beginSendFrames();
    }
}

void AugmentedEtherMac::addReceptionInReconnectState(long packetTreeId, simtime_t endRxTime)
{
    // note: packetTreeId==-1 is legal, and represents a special entry that marks the end of the reconnect state

    // housekeeping: remove expired entries from endRxTimeList
    simtime_t now = simTime();
    while (!endRxTimeList.empty() && endRxTimeList.front().endTime <= now)
        endRxTimeList.pop_front();

    // remove old entry with same packet tree ID (typically: a frame reception
    // doesn't go through but is canceled by a jam signal)
    auto i = endRxTimeList.begin();
    for ( ; i != endRxTimeList.end(); i++) {
        if (i->packetTreeId == packetTreeId) {
            endRxTimeList.erase(i);
            break;
        }
    }

    // find insertion position and insert new entry (list is ordered by endRxTime)
    for (i = endRxTimeList.begin(); i != endRxTimeList.end() && i->endTime <= endRxTime; i++)
        ;
    PkIdRxTime item(packetTreeId, endRxTime);
    endRxTimeList.insert(i, item);

    // adjust endRxMsg if needed (we'll exit reconnect mode when endRxMsg expires)
    simtime_t maxRxTime = endRxTimeList.back().endTime;
    if (endRxMsg->getArrivalTime() != maxRxTime) {
        cancelEvent(endRxMsg);
        scheduleAt(maxRxTime, endRxMsg);
    }
}

void AugmentedEtherMac::addReception(simtime_t endRxTime)
{
    numConcurrentTransmissions++;

    if (endRxMsg->getArrivalTime() < endRxTime) {
        cancelEvent(endRxMsg);
        scheduleAt(endRxTime, endRxMsg);
    }
}

void AugmentedEtherMac::processReceivedJam(EthernetJamSignal *jam)
{
    simtime_t endRxTime = simTime() + jam->getDuration();
    delete jam;

    numConcurrentTransmissions--;
    if (numConcurrentTransmissions < 0)
        throw cRuntimeError("Received JAM without message");

    if (numConcurrentTransmissions == 0 || endRxMsg->getArrivalTime() < endRxTime) {
        cancelEvent(endRxMsg);
        scheduleAt(endRxTime, endRxMsg);
    }

    processDetectedCollision();
}

void AugmentedEtherMac::processJamSignalFromNetwork(EthernetSignal *msg)
{
    EV_DETAIL << "Received " << msg << " from network.\n";

    if (!connected || disabled) {
        EV_WARN << (!connected ? "Interface is not connected" : "MAC is disabled") << " -- dropping msg " << msg << endl;
        delete msg;
        return;
    }

    // detect cable length violation in half-duplex mode
    if (!duplexMode) {
        simtime_t propagationTime = simTime() - msg->getSendingTime();
        if (propagationTime >= curEtherDescr->maxPropagationDelay) {
            throw cRuntimeError("Very long frame propagation time detected, maybe cable exceeds "
                                "maximum allowed length? (%lgs corresponds to an approx. %lgm cable)",
                    SIMTIME_STR(propagationTime),
                    SIMTIME_STR(propagationTime * SPEED_OF_LIGHT_IN_CABLE));
        }
    }

    simtime_t endRxTime = simTime() + msg->getDuration();
    EthernetJamSignal *jamMsg = dynamic_cast<EthernetJamSignal *>(msg);

    if (duplexMode && jamMsg) {
        throw cRuntimeError("Stray jam signal arrived in full-duplex mode");
    }
    else if (!duplexMode && receiveState == RX_RECONNECT_STATE) {
        long treeId = jamMsg->getAbortedPkTreeID();
        addReceptionInReconnectState(treeId, endRxTime);
        delete msg;
    }
    else if (!duplexMode && (transmitState == TRANSMITTING_STATE || transmitState == SEND_IFG_STATE)) {
        // since we're half-duplex, receiveState must be RX_IDLE_STATE (asserted at top of handleMessage)
        if (jamMsg)
            throw cRuntimeError("Stray jam signal arrived while transmitting (usual cause is cable length exceeding allowed maximum)");
    }
    else if (receiveState == RX_IDLE_STATE) {
        if (jamMsg)
            throw cRuntimeError("Stray jam signal arrived (usual cause is cable length exceeding allowed maximum)");
    }
    else {    // (receiveState==RECEIVING_STATE || receiveState==RX_COLLISION_STATE)
              // handle overlapping receptions
        processReceivedJam(jamMsg);
    }
}

void AugmentedEtherMac::processMsgFromNetwork(EthernetSignal *signal)
{
    EV_DETAIL << "Received " << signal << " from network.\n";

    if (!connected || disabled) {
        EV_WARN << (!connected ? "Interface is not connected" : "MAC is disabled") << " -- dropping msg " << signal << endl;
        if (typeid(*signal) == typeid(EthernetSignal)) {    // do not count JAM and IFG packets
            auto packet = check_and_cast<Packet *>(signal->decapsulate());
            delete signal;
            decapsulate(packet);
            PacketDropDetails details;
            details.setReason(INTERFACE_DOWN);
            emit(packetDroppedSignal, packet, &details);
            delete packet;
            numDroppedIfaceDown++;
        }
        else
            delete signal;

        return;
    }

    if (signal->getSrcMacFullDuplex() != duplexMode)
        throw cRuntimeError("Ethernet misconfiguration: MACs on the same link must be all in full duplex mode, or all in half-duplex mode");

    // detect cable length violation in half-duplex mode
    if (!duplexMode) {
        simtime_t propagationTime = simTime() - signal->getSendingTime();
        if (propagationTime >= curEtherDescr->maxPropagationDelay) {
            throw cRuntimeError("Very long frame propagation time detected, maybe cable exceeds "
                                "maximum allowed length? (%lgs corresponds to an approx. %lgm cable)",
                    SIMTIME_STR(propagationTime),
                    SIMTIME_STR(propagationTime * SPEED_OF_LIGHT_IN_CABLE));
        }
    }

    simtime_t endRxTime = simTime() + signal->getDuration();

    if (!duplexMode && receiveState == RX_RECONNECT_STATE) {
        long treeId = signal->getTreeId();
        addReceptionInReconnectState(treeId, endRxTime);
        delete signal;
    }
    else if (!duplexMode && (transmitState == TRANSMITTING_STATE || transmitState == SEND_IFG_STATE)) {
        // since we're half-duplex, receiveState must be RX_IDLE_STATE (asserted at top of handleMessage)
        // set receive state and schedule end of reception
        changeReceptionState(RX_COLLISION_STATE);

        addReception(endRxTime);
        delete signal;

        EV_DETAIL << "Transmission interrupted by incoming frame, handling collision\n";
        cancelEvent((transmitState == TRANSMITTING_STATE) ? endTxMsg : endIFGMsg);

        EV_DETAIL << "Transmitting jam signal\n";
        sendJamSignal();    // backoff will be executed when jamming finished

        numCollisions++;
        emit(collisionSignal, 1L);
    }
    else if (receiveState == RX_IDLE_STATE) {
        channelBusySince = simTime();
        EV_INFO << "Reception of " << signal << " started.\n";
        scheduleEndRxPeriod(signal);
    }
    else if (receiveState == RECEIVING_STATE && endRxMsg->getArrivalTime() - simTime() < curEtherDescr->halfBitTime)
    {
        // With the above condition we filter out "false" collisions that may occur with
        // back-to-back frames. That is: when "beginning of frame" message (this one) occurs
        // BEFORE "end of previous frame" event (endRxMsg) -- same simulation time,
        // only wrong order.

        EV_DETAIL << "Back-to-back frames: completing reception of current frame, starting reception of next one\n";

        // complete reception of previous frame
        cancelEvent(endRxMsg);
        frameReceptionComplete();

        // calculate usability
        totalSuccessfulRxTxTime += simTime() - channelBusySince;
        channelBusySince = simTime();

        // start receiving next frame
        scheduleEndRxPeriod(signal);
    }
    else {    // (receiveState==RECEIVING_STATE || receiveState==RX_COLLISION_STATE)
              // handle overlapping receptions
        // EtherFrame or EtherPauseFrame
        EV_DETAIL << "Overlapping receptions -- setting collision state\n";
        addReception(endRxTime);
        // delete collided frames: arrived frame as well as the one we're currently receiving
        delete signal;
        processDetectedCollision();
    }
}

void AugmentedEtherMac::processDetectedCollision()
{
    if (receiveState != RX_COLLISION_STATE) {
        delete frameBeingReceived;
        frameBeingReceived = nullptr;

        numCollisions++;
        emit(collisionSignal, 1L);
        // go to collision state
        changeReceptionState(RX_COLLISION_STATE);
    }
}

void AugmentedEtherMac::handleEndIFGPeriod()
{
    if (transmitState != WAIT_IFG_STATE && transmitState != SEND_IFG_STATE)
        throw cRuntimeError("Not in WAIT_IFG_STATE at the end of IFG period");

    currentSendPkTreeID = 0;

    EV_DETAIL << "IFG elapsed in AugmentedEtherMac\n";

    if (frameBursting && (transmitState != SEND_IFG_STATE)) {
        bytesSentInBurst = B(0);
        framesSentInBurst = 0;
    }

    // End of IFG period, okay to transmit, if Rx idle OR duplexMode ( checked in startFrameTransmission(); )

    // send frame to network
    beginSendFrames();
}

B AugmentedEtherMac::calculateMinFrameLength()
{
    bool inBurst = frameBursting && framesSentInBurst;
    B minFrameLength = duplexMode ? MIN_ETHERNET_FRAME_BYTES : (inBurst ? curEtherDescr->frameInBurstMinBytes : curEtherDescr->halfDuplexFrameMinBytes);

    return minFrameLength;
}

B AugmentedEtherMac::calculatePaddedFrameLength(Packet *frame)
{
    B minFrameLength = calculateMinFrameLength();
    return std::max(minFrameLength, B(frame->getDataLength()));
}

void AugmentedEtherMac::startFrameTransmission()
{
    ASSERT(currentTxFrame);

    EV_INFO << "Transmission of " << currentTxFrame << " started.\n";

    Packet *frame = currentTxFrame->dup();
    const auto& hdr = frame->peekAtFront<EthernetMacHeader>();

    ASSERT(hdr);
    ASSERT(!hdr->getSrc().isUnspecified());

    B minFrameLengthWithExtension = calculateMinFrameLength();
    B extensionLength = minFrameLengthWithExtension > frame->getDataLength() ? (minFrameLengthWithExtension - frame->getDataLength()) : B(0);

    // add preamble and SFD (Starting Frame Delimiter), then send out
    encapsulate(frame);

    B sentFrameByteLength = frame->getDataLength() + extensionLength;
    auto oldPacketProtocolTag = frame->removeTag<PacketProtocolTag>();
    frame->clearTags();
    auto newPacketProtocolTag = frame->addTag<PacketProtocolTag>();
    *newPacketProtocolTag = *oldPacketProtocolTag;
    delete oldPacketProtocolTag;
    auto signal = new EthernetSignal(frame->getName());
    signal->setSrcMacFullDuplex(duplexMode);
    currentSendPkTreeID = signal->getTreeId();
    if (sendRawBytes) {
        auto rawFrame = new Packet(frame->getName(), frame->peekAllAsBytes());
        rawFrame->copyTags(*frame);
        signal->encapsulate(rawFrame);
        delete frame;
    }
    else
        signal->encapsulate(frame);
    signal->addByteLength(extensionLength.get());
    send(signal, physOutGate);

    // check for collisions (there might be an ongoing reception which we don't know about, see below)
    if (!duplexMode && receiveState != RX_IDLE_STATE) {
        // During the IFG period the hardware cannot listen to the channel,
        // so it might happen that receptions have begun during the IFG,
        // and even collisions might be in progress.
        //
        // But we don't know of any ongoing transmission so we blindly
        // start transmitting, immediately collide and send a jam signal.
        //
        EV_DETAIL << "startFrameTransmission(): sending JAM signal.\n";
        printState();

        sendJamSignal();
        // numConcurrentRxTransmissions stays the same: +1 transmission, -1 jam

        if (receiveState == RECEIVING_STATE) {
            delete frameBeingReceived;
            frameBeingReceived = nullptr;

            numCollisions++;
            emit(collisionSignal, 1L);
        }
        // go to collision state
        changeReceptionState(RX_COLLISION_STATE);
    }
    else {
        // no collision
        scheduleEndTxPeriod(sentFrameByteLength);

        // only count transmissions in totalSuccessfulRxTxTime if channel is half-duplex
        if (!duplexMode)
            channelBusySince = simTime();
    }
}

void AugmentedEtherMac::handleEndTxPeriod()
{

    if (!use_pfc && (txQueue->pfc_status != txQueue->PFC_RESUME || txQueue->neighbor_pfc_status != txQueue->PFC_RESUME))
        throw cRuntimeError("handleEndTxPeriod: !use_pfc && (txQueue->pfc_status != txQueue->PFC_RESUME || txQueue->neighbor_pfc_status != txQueue->PFC_RESUME)");

    // we only get here if transmission has finished successfully, without collision
    if (transmitState != TRANSMITTING_STATE || (!duplexMode && receiveState != RX_IDLE_STATE))
        throw cRuntimeError("End of transmission, and incorrect state detected");

    currentSendPkTreeID = 0;

    if (currentTxFrame == nullptr)
        throw cRuntimeError("Frame under transmission cannot be found");

    numFramesSent++;
    numBytesSent += currentTxFrame->getByteLength();
    emit(packetSentToLowerSignal, currentTxFrame);    //consider: emit with start time of frame

    const auto& header = currentTxFrame->peekAtFront<EthernetMacHeader>();
    if (header->getTypeOrLength() == ETHERTYPE_FLOW_CONTROL) {
        const auto& controlFrame = currentTxFrame->peekDataAt<EthernetControlFrame>(header->getChunkLength(), b(-1));
        if (controlFrame->getOpCode() == ETHERNET_CONTROL_PAUSE) {
            const auto& pauseFrame = dynamicPtrCast<const EthernetPauseFrame>(controlFrame);
            numPauseFramesSent++;
            emit(txPausePkUnitsSignal, pauseFrame->getPauseTime());
        }
    }

    EV_INFO << "Transmission of " << currentTxFrame << " successfully completed.\n";
    deleteCurrentTxFrame();
    lastTxFinishTime = simTime();

    // note: cannot be moved into handleEndIFGPeriod(), because in burst mode we need to know whether to send filled IFG or not
    if (!txQueue->isEmpty())
        popTxQueue();

    // only count transmissions in totalSuccessfulRxTxTime if channel is half-duplex
    if (!duplexMode) {
        simtime_t dt = simTime() - channelBusySince;
        totalSuccessfulRxTxTime += dt;
    }

    backoffs = 0;

    // check for and obey received PAUSE frames after each transmission
    if (pauseUnitsRequested > 0) {
        // if we received a PAUSE frame recently, go into PAUSE state
        EV_DETAIL << "Going to PAUSE mode for " << pauseUnitsRequested << " time units\n";
        scheduleEndPausePeriod(pauseUnitsRequested);
        pauseUnitsRequested = 0;
    }
    else {
        EV_DETAIL << "Start IFG period\n";
        // in endIFGperiod function we call beginSendFrames which then checks if is_allowed_to_pop_packet_by_PFC
        scheduleEndIFGPeriod();
        fillIFGIfInBurst();
    }
}

void AugmentedEtherMac::scheduleEndRxPeriod(EthernetSignal *frame)
{
    ASSERT(frameBeingReceived == nullptr);
    ASSERT(!endRxMsg->isScheduled());

    frameBeingReceived = frame;
    changeReceptionState(RECEIVING_STATE);
    addReception(simTime() + frame->getDuration());
}

void AugmentedEtherMac::handleEndRxPeriod()
{
    simtime_t dt = simTime() - channelBusySince;

    EV << "AugmentedEtherMac::handleEndRxPeriod(): " << receiveState << endl;

    switch (receiveState) {
        case RECEIVING_STATE:
            frameReceptionComplete();
            totalSuccessfulRxTxTime += dt;
            break;

        case RX_COLLISION_STATE:
            EV_DETAIL << "Incoming signals finished after collision\n";
            totalCollisionTime += dt;
            break;

        case RX_RECONNECT_STATE:
            EV_DETAIL << "Incoming signals finished or reconnect time elapsed after reconnect\n";
            endRxTimeList.clear();
            break;

        default:
            throw cRuntimeError("model error: invalid receiveState %d", receiveState);
    }

    changeReceptionState(RX_IDLE_STATE);
    numConcurrentTransmissions = 0;

    if (transmitState == TX_IDLE_STATE)
        scheduleEndIFGPeriod();
}

void AugmentedEtherMac::handleEndBackoffPeriod()
{
    if (transmitState != BACKOFF_STATE)
        throw cRuntimeError("At end of BACKOFF and not in BACKOFF_STATE");

    if (currentTxFrame == nullptr)
        throw cRuntimeError("At end of BACKOFF and no frame to transmit");

    if (receiveState == RX_IDLE_STATE) {
        EV_DETAIL << "Backoff period ended, wait IFG\n";
        scheduleEndIFGPeriod();
    }
    else {
        EV_DETAIL << "Backoff period ended but channel is not free, idling\n";
        changeTransmissionState(TX_IDLE_STATE);
    }
}

void AugmentedEtherMac::sendJamSignal()
{
    if (currentSendPkTreeID == 0)
        throw cRuntimeError("Model error: sending JAM while not transmitting");

    EthernetJamSignal *jam = new EthernetJamSignal("JAM_SIGNAL");
    jam->setByteLength(B(JAM_SIGNAL_BYTES).get());
    jam->setAbortedPkTreeID(currentSendPkTreeID);

    transmissionChannel->forceTransmissionFinishTime(SIMTIME_ZERO);
    //emit(packetSentToLowerSignal, jam);
    send(jam, physOutGate);

    scheduleAt(transmissionChannel->getTransmissionFinishTime(), endJammingMsg);
    changeTransmissionState(JAMMING_STATE);
}

void AugmentedEtherMac::handleEndJammingPeriod()
{
    if (transmitState != JAMMING_STATE)
        throw cRuntimeError("At end of JAMMING but not in JAMMING_STATE");

    EV_DETAIL << "Jamming finished, executing backoff\n";
    handleRetransmission();
}

void AugmentedEtherMac::handleRetransmission()
{

    if (!use_pfc && (txQueue->pfc_status != txQueue->PFC_RESUME || txQueue->neighbor_pfc_status != txQueue->PFC_RESUME))
        throw cRuntimeError("handleRetransmission: !use_pfc && (txQueue->pfc_status != txQueue->PFC_RESUME || txQueue->neighbor_pfc_status != txQueue->PFC_RESUME)");

    if (++backoffs > MAX_ATTEMPTS) {
        EV_DETAIL << "Number of retransmit attempts of frame exceeds maximum, cancelling transmission of frame\n";
        PacketDropDetails details;
        details.setReason(RETRY_LIMIT_REACHED);
        details.setLimit(MAX_ATTEMPTS);
        dropCurrentTxFrame(details);
        changeTransmissionState(TX_IDLE_STATE);
        backoffs = 0;
        if (!txQueue->isEmpty())
            popTxQueue();
        beginSendFrames();
        return;
    }

    EV_DETAIL << "Executing backoff procedure\n";
    int backoffRange = (backoffs >= BACKOFF_RANGE_LIMIT) ? 1024 : (1 << backoffs);
    int slotNumber = intuniform(0, backoffRange - 1);

    scheduleAt(simTime() + slotNumber * curEtherDescr->slotTime, endBackoffMsg);
    changeTransmissionState(BACKOFF_STATE);
    emit(backoffSlotsGeneratedSignal, slotNumber);

    numBackoffs++;
}

void AugmentedEtherMac::printState()
{
#define CASE(x)    case x: \
        EV_DETAIL << #x; break

    EV_DETAIL << "transmitState: ";
    switch (transmitState) {
        CASE(TX_IDLE_STATE);
        CASE(WAIT_IFG_STATE);
        CASE(SEND_IFG_STATE);
        CASE(TRANSMITTING_STATE);
        CASE(JAMMING_STATE);
        CASE(BACKOFF_STATE);
        CASE(PAUSE_STATE);
    }

    EV_DETAIL << ",  receiveState: ";
    switch (receiveState) {
        CASE(RX_IDLE_STATE);
        CASE(RECEIVING_STATE);
        CASE(RX_COLLISION_STATE);
        CASE(RX_RECONNECT_STATE);
    }

    EV_DETAIL << ",  backoffs: " << backoffs;
    EV_DETAIL << ",  numConcurrentRxTransmissions: " << numConcurrentTransmissions;
    EV_DETAIL << ",  queueLength: " << txQueue->getNumPackets();
    EV_DETAIL << endl;

#undef CASE
}

void AugmentedEtherMac::finish()
{
    EtherMacBase::finish();

    simtime_t t = simTime();
    simtime_t totalChannelIdleTime = t - totalSuccessfulRxTxTime - totalCollisionTime;
    recordScalar("rx channel idle (%)", 100 * (totalChannelIdleTime / t));
    recordScalar("rx channel utilization (%)", 100 * (totalSuccessfulRxTxTime / t));
    recordScalar("rx channel collision (%)", 100 * (totalCollisionTime / t));
    recordScalar("collisions", numCollisions);
    recordScalar("backoffs", numBackoffs);
}

void AugmentedEtherMac::handleEndPausePeriod()
{
    if (transmitState != PAUSE_STATE)
        throw cRuntimeError("At end of PAUSE and not in PAUSE_STATE");

    EV_DETAIL << "Pause finished, resuming transmissions\n";
    beginSendFrames();
}

void AugmentedEtherMac::frameReceptionComplete()
{
    EV << "AugmentedEtherMac::frameReceptionComplete" << endl;
    EthernetSignal *signal = frameBeingReceived;
    frameBeingReceived = nullptr;

    if (dynamic_cast<EthernetFilledIfgSignal *>(signal) != nullptr) {
        EV_WARN << "dynamic_cast<EthernetFilledIfgSignal *>(signal) != nullptr" << endl;
        delete signal;
        return;
    }
    if (signal->getSrcMacFullDuplex() != duplexMode)
        throw cRuntimeError("Ethernet misconfiguration: MACs on the same link must be all in full duplex mode, or all in half-duplex mode");

    bool hasBitError = signal->hasBitError();
    auto packet = check_and_cast<Packet *>(signal->decapsulate());
    delete signal;
    decapsulate(packet);
    emit(packetReceivedFromLowerSignal, packet);

    const auto& frame = packet->peekAtFront<EthernetMacHeader>();
    int pfc_signal = frame->getPfc_signal();
    if (pfc_signal != -1) {
        EV << "Packet is PFC signal:" << pfc_signal << endl;
        if (!use_pfc)
            throw cRuntimeError("PFC signal received while use_pfc is false");
        std::string packet_name = packet->getName();
        if (packet_name.compare("PFCSIG") != 0)
            throw cRuntimeError("Ethernet header says pfc signal but packet name doesn't");
//        std::cout << simTime() << ": " << getFullPath() << endl;
        if (pfc_signal == txQueue->PFC_STOP) {
            EV << "PFC: Stop sending..." << endl;
            txQueue->pfc_status = txQueue->PFC_STOP;
//            std::cout << "Signalled to stop sending..." << endl;
        } else if (pfc_signal == txQueue->PFC_RESUME) {
            EV << "PFC: Resume sending..." << endl;
            txQueue->pfc_status = txQueue->PFC_RESUME;
//            std::cout << "Signalled to resume sending..." << endl;

//            if (!((duplexMode || receiveState == RX_IDLE_STATE) && transmitState == TX_IDLE_STATE)) {
//                std::cout << "duplexMode: " << duplexMode << endl;
//                std::cout << "receiveState: " << receiveState << endl;
//                std::cout << "transmitState: " << transmitState << endl;
//                throw cRuntimeError("PFC Resume: !((duplexMode || receiveState == RX_IDLE_STATE) && transmitState == TX_IDLE_STATE)");
//            }

            if (!currentTxFrame && !txQueue->isEmpty())
                popTxQueue();

            if ((duplexMode || receiveState == RX_IDLE_STATE) && transmitState == TX_IDLE_STATE) {
                EV_DETAIL << "No incoming carrier signals detected, frame clear to send\n";
                beginSendFrames();
            }
        }
//        std::cout << "--------------" << endl;
        delete packet;
        return;
    }

    std::string packet_name = packet->getName();
    bool is_dcqcn_cnp = (packet_name.find("dcqcn_cnp") != std::string::npos);

    if (!is_dcqcn_cnp && (hasBitError || !verifyCrcAndLength(packet))) {
        EV_WARN << "hasBitError || !verifyCrcAndLength(packet)" << endl;
        numDroppedBitError++;
        PacketDropDetails details;
        details.setReason(INCORRECTLY_RECEIVED);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }

    if (dropFrameNotForUs(packet, frame))
        return;

    if (frame->getTypeOrLength() == ETHERTYPE_FLOW_CONTROL) {
        processReceivedControlFrame(packet);
    }
    else {
        EV_INFO << "Reception of " << frame << " successfully completed.\n";

        processReceivedDataFrame(packet);
    }
}

bool AugmentedEtherMac::isPacketAppropriateForMulticast(std::string pkt_name) {
    if (pkt_name.compare("data") == 0)
        return true;

    if (pkt_name.compare("treeInit") == 0)
        return true;

    if (pkt_name.compare("treeFin") == 0)
        return true;

    return false;
}

void AugmentedEtherMac::generate_base_signal_packet(Packet *packet_dup) {
    EV << "AugmentedEtherMac::generate_base_signal_packet called: " << packet_dup->str() << endl;
    base_signal_packet = new Packet("PFCSIG");
//    b position = packet_dup->getFrontOffset();
    packet_dup->setFrontIteratorPosition(b(0));
//    base_signal_packet->insertAtBack(packet_dup->removeAtFront<EthernetPhyHeader>());
    auto eth_header = packet_dup->removeAtFront<EthernetMacHeader>();
    eth_header->setOrca_pkt_seen_agent(false);
    eth_header->setRsbf_redundant_send(false);
    eth_header->setDst_tor_idx(-1);
    eth_header->setIs_drop_notif(false);
    base_signal_packet->insertAtBack(eth_header);
//    base_signal_packet->setFrontIteratorPosition(position);
    base_signal_packet->setFrontIteratorPosition(b(0));
    // input packet is duplicate... delete it
    delete packet_dup;
    EV << "base_signal_packet is " << base_signal_packet->str() << endl;
    return;
}

void AugmentedEtherMac::generate_and_send_pfc_signal(uint8 signal) {
    Enter_Method("generate_and_send_pfc_signal");
    if (!base_signal_packet)
        throw cRuntimeError("generate_and_send_pfc_signal called while base_signal_packet is nullptr");

    EV << "AugmentedEtherMac::generate_and_send_pfc_signal: " << signal << endl;

    b position = base_signal_packet->getFrontOffset();
    base_signal_packet->setFrontIteratorPosition(b(0));
//    auto phy_header = base_signal_packet->removeAtFront<EthernetPhyHeader>();
    auto eth_header = base_signal_packet->removeAtFront<EthernetMacHeader>();
    eth_header->setPfc_signal(signal);
    base_signal_packet->insertAtFront(eth_header);
//    base_signal_packet->insertAtFront(phy_header);
    base_signal_packet->setFrontIteratorPosition(position);

    txQueue->pushPacket(base_signal_packet->dup());

    txQueue->neighbor_pfc_status = signal;

    emit(pfcGeneratedSignal, signal);

    if (!currentTxFrame) {
        popTxQueue();
    }

    // queue might be idle... if so, start sending
    // we don't use begin sending because pfc signal should not be STOPPED by PFC
    ASSERT(currentTxFrame != nullptr);
    if ((duplexMode || receiveState == RX_IDLE_STATE) && transmitState == TX_IDLE_STATE) {
        EV << "No incoming carrier signals detected, frame clear to send\n";
        startFrameTransmission();
    } else {
        EV << "NOT idle..." << endl;
        EV << "transmitState: " << transmitState << endl;
    }
}


void AugmentedEtherMac::processReceivedDataFrame(Packet *packet)
{

    // handle receiving pfc signal
    auto eth_header = packet->peekAtFront<EthernetMacHeader>();
    int pfc_signal = eth_header->getPfc_signal();
    std::string packet_name = packet->getName();
    if (pfc_signal != -1) {
        delete packet;
        throw cRuntimeError("Expected data packet but got pfc signal!");
    }

    // statistics
    unsigned long curBytes = packet->getByteLength();
    numFramesReceivedOK++;
    numBytesReceivedOK += curBytes;
    emit(rxPkOkSignal, packet);

    packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::ethernetMac);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ethernetMac);
    EV << "1) Input port idx is " << port_idx << endl;
    packet->addTag<InputPortTag>()->setInput_port_idx(port_idx);

    if (!base_signal_packet) {
//        if (packet_name.find("dcqcn_cnp") != std::string::npos)
//            throw cRuntimeError("generate_base_signal_packet is being called for a DCQCN CNP packet");
        generate_base_signal_packet(packet->dup());
    }

    if (interfaceEntry)
        packet->addTagIfAbsent<InterfaceInd>()->setInterfaceId(interfaceEntry->getInterfaceId());


    if (isPacketAppropriateForMulticast(packet->getName())) {
        b position = packet->getFrontOffset();
        packet->setFrontIteratorPosition(b(0));
        auto phy_header = packet->removeAtFront<EthernetPhyHeader>();
        auto eth_header = packet->removeAtFront<EthernetMacHeader>();
        eth_header->insertInput_ports_passed(getParentModule()->getIndex());
//        std::cout << "getInput_ports_passedArraySize(): " << eth_header->getInput_ports_passedArraySize() << endl;
        packet->insertAtFront(eth_header);
        packet->insertAtFront(phy_header);
        packet->setFrontIteratorPosition(position);
    }

    numFramesPassedToHL++;
    emit(packetSentToUpperSignal, packet);
    // pass up to upper layer
    EV_INFO << "Sending " << packet << " to upper layer.\n";
    send(packet, "upperLayerOut");
}

void AugmentedEtherMac::processReceivedControlFrame(Packet *packet)
{
    packet->popAtFront<EthernetMacHeader>();
    const auto& controlFrame = packet->peekAtFront<EthernetControlFrame>();

    if (controlFrame->getOpCode() == ETHERNET_CONTROL_PAUSE) {
        const auto& pauseFrame = packet->peekAtFront<EthernetPauseFrame>();
        int pauseUnits = pauseFrame->getPauseTime();

        numPauseFramesRcvd++;
        emit(rxPausePkUnitsSignal, pauseUnits);

        if (transmitState == TX_IDLE_STATE) {
            EV_DETAIL << "PAUSE frame received, pausing for " << pauseUnitsRequested << " time units\n";
            if (pauseUnits > 0)
                scheduleEndPausePeriod(pauseUnits);
        }
        else if (transmitState == PAUSE_STATE) {
            EV_DETAIL << "PAUSE frame received, pausing for " << pauseUnitsRequested
                      << " more time units from now\n";
            cancelEvent(endPauseMsg);

            if (pauseUnits > 0)
                scheduleEndPausePeriod(pauseUnits);
        }
        else {
            // transmitter busy -- wait until it finishes with current frame (endTx)
            // and then it'll go to PAUSE state
            EV_DETAIL << "PAUSE frame received, storing pause request\n";
            pauseUnitsRequested = pauseUnits;
        }
    }
    delete packet;
}

void AugmentedEtherMac::scheduleEndIFGPeriod()
{
    changeTransmissionState(WAIT_IFG_STATE);
    simtime_t endIFGTime = simTime() + (b(INTERFRAME_GAP_BITS).get() / curEtherDescr->txrate);
    scheduleAt(endIFGTime, endIFGMsg);
}

void AugmentedEtherMac::fillIFGIfInBurst()
{
    if (!frameBursting)
        return;

    EV_TRACE << "fillIFGIfInBurst(): t=" << simTime() << ", framesSentInBurst=" << framesSentInBurst << ", bytesSentInBurst=" << bytesSentInBurst << endl;

    if (currentTxFrame
        && endIFGMsg->isScheduled()
        && (transmitState == WAIT_IFG_STATE)
        && (simTime() == lastTxFinishTime)
        && (simTime() == endIFGMsg->getSendingTime())
        && (framesSentInBurst > 0)
        && (framesSentInBurst < curEtherDescr->maxFramesInBurst)
        && (bytesSentInBurst + INTERFRAME_GAP_BITS + PREAMBLE_BYTES + SFD_BYTES + calculatePaddedFrameLength(currentTxFrame)
            <= curEtherDescr->maxBytesInBurst)
        )
    {
        throw cRuntimeError("SEPEHR: fillIFGIfInBurst");
        EthernetFilledIfgSignal *gap = new EthernetFilledIfgSignal("FilledIFG");
        bytesSentInBurst += B(gap->getByteLength());
        currentSendPkTreeID = gap->getTreeId();
        send(gap, physOutGate);
        changeTransmissionState(SEND_IFG_STATE);
        cancelEvent(endIFGMsg);
        scheduleAt(transmissionChannel->getTransmissionFinishTime(), endIFGMsg);
    }
    else {
        bytesSentInBurst = B(0);
        framesSentInBurst = 0;
    }
}

void AugmentedEtherMac::scheduleEndTxPeriod(B sentFrameByteLength)
{
    // update burst variables
    if (frameBursting) {
        bytesSentInBurst += sentFrameByteLength;
        framesSentInBurst++;
    }

    scheduleAt(transmissionChannel->getTransmissionFinishTime(), endTxMsg);
    changeTransmissionState(TRANSMITTING_STATE);
}

void AugmentedEtherMac::scheduleEndPausePeriod(int pauseUnits)
{
    // length is interpreted as 512-bit-time units
    simtime_t pausePeriod = pauseUnits * PAUSE_UNIT_BITS / curEtherDescr->txrate;
    scheduleAt(simTime() + pausePeriod, endPauseMsg);
    changeTransmissionState(PAUSE_STATE);
}

void AugmentedEtherMac::beginSendFrames()
{
    EV << "AugmentedEtherMac::beginSendFrames" << endl;
    if (currentTxFrame && txQueue->is_allowed_to_send_by_PFC(currentTxFrame)) {
        // Other frames are queued, therefore wait IFG period and transmit next frame
        EV_DETAIL << "Will transmit next frame in output queue after IFG period\n";
        startFrameTransmission();
    }
    else {
        // No more frames, set transmitter to idle
        changeTransmissionState(TX_IDLE_STATE);
        if (txQueue->pfc_status != txQueue->PFC_RESUME)
            EV << "Not allowed to send by PFC. Changing to IDLE" << endl;
        else
            EV_DETAIL << "No more frames to send, transmitter set to idle\n";
    }
}

bool AugmentedEtherMac::is_queue_full(b packet_length, std::string queue_path)
{
    EV << "AugmentedEtherMac::is_queue_full" << endl;
    EV << "queue path is: " << queue_path << endl;
    if (buffer != nullptr ||
            send_header_of_dropped_packet_to_receiver ||
            use_v2_pifo ||
            use_vertigo_prio_queue ||
            use_pfabric) {
//        Priority queues, txQueue is actually the scheduler
        PacketQueue* queue;
        if (use_bolt_queue && (use_v2_pifo || use_vertigo_prio_queue))
            queue = check_and_cast<V2PIFOBoltQueue *>(getModuleByPath(queue_path.c_str()));
        else if (use_bolt_with_vertigo_queue && (use_v2_pifo || use_vertigo_prio_queue))
            queue = check_and_cast<V2PIFO *>(getModuleByPath(queue_path.c_str()));
        else if (use_v2_pifo) {
            queue = check_and_cast<V2PIFO *>(getModuleByPath(queue_path.c_str()));
        } else if (use_pfabric)
            queue = check_and_cast<pFabric *>(getModuleByPath(queue_path.c_str()));
        else if (use_vertigo_prio_queue) {
            switch (v2pifo_queue_type) {
                case V2PIFOPRIO:
                    queue = check_and_cast<V2PIFOPrioQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                case V2PIFOCANARY:
                    queue = check_and_cast<V2PIFOCanaryQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                default:
                    throw cRuntimeError("Unknown prio queue");
            }
        }
        return queue->is_queue_full(packet_length, on_the_way_packet_num, on_the_way_packet_length);
    } else {
        bool is_queue_full = (txQueue->getMaxNumPackets() != -1 && txQueue->getNumPackets() + on_the_way_packet_num >= txQueue->getMaxNumPackets()) ||
                (txQueue->getMaxTotalLength() != b(-1) && (txQueue->getTotalLength() + on_the_way_packet_length + packet_length) > txQueue->getMaxTotalLength());
        EV << "Checking if queue is full" << endl;
        if (txQueue->getMaxNumPackets() != -1)
            EV << "The queue capacity is " << txQueue->getMaxNumPackets() << ", There are currently " << txQueue->getNumPackets() << " packets inside the queue and " << on_the_way_packet_num << " packets on the way. Is the queue full? " << is_queue_full << endl;
        else if (txQueue->getMaxTotalLength() != b(-1))
            EV << "The queue capacity is " << txQueue->getMaxTotalLength() << ", Queue length is " << txQueue->getTotalLength() << " and packet length is " << packet_length << " and " << on_the_way_packet_length << " bytes on the way. Is the queue full? " << is_queue_full << endl;
        return is_queue_full;
    }
}

bool AugmentedEtherMac::is_queue_over_v2_threshold(b packet_length, std::string queue_path, Packet* packet)
{
    EV << "AugmentedEtherMac::is_queue_over_v2_threshold" << endl;
    EV << "queue path is: " << queue_path << endl;
    if (use_bolt_with_vertigo_queue)
        throw cRuntimeError("is_queue_over_v2_threshold called while use_bolt_with_vertigo_queue. Not possible!");
    if (use_vertigo_prio_queue) {
//        Priority queues, txQueue is actually the scheduler
        PacketQueue* queue;
        if (use_bolt_queue) {
            queue = check_and_cast<V2PIFOBoltQueue *>(getModuleByPath(queue_path.c_str()));
        } else {
            switch (v2pifo_queue_type) {
                case V2PIFOPRIO:
                    queue = check_and_cast<V2PIFOPrioQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                case V2PIFOCANARY:
                    queue = check_and_cast<V2PIFOCanaryQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                default:
                    throw cRuntimeError("Unknown prio queue");
            }
        }
        return queue->is_over_v2_threshold_full(packet_length, packet, on_the_way_packet_num, on_the_way_packet_length);
    }
    throw cRuntimeError("Is not using queues with v2 threshold");
}

bool AugmentedEtherMac::is_packet_tag_larger_than_last_packet(std::string queue_path, Packet* packet)
{
    EV << "AugmentedEtherMac::is_packet_tag_larger_than_last_packet" << endl;
    EV << "queue path is: " << queue_path << endl;
    if (use_bolt_with_vertigo_queue)
        throw cRuntimeError("is_packet_tag_larger_than_last_packet called while use_bolt_with_vertigo_queue. Not possible!");
    if (use_vertigo_prio_queue) {
//        Priority queues, txQueue is actually the scheduler
        PacketQueue* queue;
        if (use_bolt_queue) {
            queue = check_and_cast<V2PIFOBoltQueue *>(getModuleByPath(queue_path.c_str()));
        } else {
            switch (v2pifo_queue_type) {
                case V2PIFOPRIO:
                    queue = check_and_cast<V2PIFOPrioQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                case V2PIFOCANARY:
                    queue = check_and_cast<V2PIFOCanaryQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                default:
                    throw cRuntimeError("Unknown prio queue");
            }
        }
        return queue->is_packet_tag_larger_than_last_packet(packet);
    }
    throw cRuntimeError("Is not using queues with v2 threshold");
}

long AugmentedEtherMac::get_queue_occupancy(std::string queue_path)
{
    EV << "AugmentedEtherMac::get_queue_occupancy" << endl;
    if (send_header_of_dropped_packet_to_receiver ||
            use_v2_pifo ||
            use_pfabric ||
            use_vertigo_prio_queue ||
            buffer != nullptr) {
        // Priority queues, txQueue is actually the scheduler
        PacketQueue* queue;
        if (use_bolt_queue && (use_vertigo_prio_queue || use_v2_pifo)) {
            queue = check_and_cast<V2PIFOBoltQueue *>(getModuleByPath(queue_path.c_str()));
        } else if (use_bolt_with_vertigo_queue && (use_vertigo_prio_queue || use_v2_pifo)) {
            queue = check_and_cast<V2PIFO *>(getModuleByPath(queue_path.c_str()));
        }
        else if (use_v2_pifo) {
            queue = check_and_cast<V2PIFO *>(getModuleByPath(queue_path.c_str()));
        } else if (use_pfabric)
            queue = check_and_cast<pFabric *>(getModuleByPath(queue_path.c_str()));
        else if (use_vertigo_prio_queue) {
            switch (v2pifo_queue_type) {
                case V2PIFOPRIO:
                    queue = check_and_cast<V2PIFOPrioQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                case V2PIFOCANARY:
                    queue = check_and_cast<V2PIFOCanaryQueue *>(getModuleByPath(queue_path.c_str()));
                    break;
                default:
                    throw cRuntimeError("Unknown prio queue");
            }
        }
        return queue->get_queue_occupancy(on_the_way_packet_num, on_the_way_packet_length);
    } else {
        if (txQueue->getMaxNumPackets() != -1) {
            return (txQueue->getNumPackets() + on_the_way_packet_num);
        }
        else if (txQueue->getMaxTotalLength() != b(-1)) {
            return (txQueue->getTotalLength() + on_the_way_packet_length).get();
        } else {
            throw cRuntimeError("No queue capacity specified! WTF?");
        }
    }

}


void AugmentedEtherMac::add_on_the_way_packet(b packet_length, bool is_v2_dropped_packet_header)
{
    EV << "AugmentedEtherMac::add_on_the_way_packet" << endl;
//    std::cout << getFullPath() << ": on_the_way_packet_length" << endl;
    if (!send_header_of_dropped_packet_to_receiver || !is_v2_dropped_packet_header) {
        on_the_way_packet_length += packet_length;
        on_the_way_packet_num += 1;
        if (buffer != nullptr) {
            buffer->on_the_way_packet_length += packet_length;
            buffer->on_the_way_packet_num += 1;
        }
//        std::cout << "packet len: " << packet_length << ", on_the_way_packet_num: " << on_the_way_packet_num << endl;
    }
}

double AugmentedEtherMac::get_link_util() {
    if (!bw_set)
        throw cRuntimeError("AugmentedEtherMac::get_link_util() --> bw is not set!");
    return transmissionChannel->getNominalDatarate();
}
