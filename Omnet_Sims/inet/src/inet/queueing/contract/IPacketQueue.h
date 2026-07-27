//
// Copyright (C) OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see http://www.gnu.org/licenses/.
//

#ifndef __INET_IPACKETQUEUE_H
#define __INET_IPACKETQUEUE_H

#include "inet/queueing/contract/IPacketCollection.h"
#include "inet/queueing/contract/IPassivePacketSink.h"
#include "inet/queueing/contract/IPassivePacketSource.h"

namespace inet {
namespace queueing {

/**
 * This class defines the interface for packet queues.
 */
class INET_API IPacketQueue : public IPacketCollection, public IPassivePacketSink, public IPassivePacketSource
{

public:
    virtual void pushPacketAfter(Packet *where, Packet *packet) {};
    // pfc
    enum PortPFCStatus { PFC_RESUME = 1, PFC_STOP};
    // this indicates if the port itself is paused or resumed
    int16 pfc_status = PFC_RESUME;
    // this indicates if the port sent pause/resume messages to the incoming data from the neighbor (is neighbor sending data or not)
    int16 neighbor_pfc_status = PFC_RESUME;
    B pfc_bytes_counter = B(0);
    unsigned long pfc_stop_sent_counter = 0;

    // just used for pfc
    virtual bool is_allowed_to_send_by_PFC(Packet* current_tx_packet) { throw cRuntimeError("is_allowed_to_pop_packet_by_PFC not implemented.");};

    virtual void insert_front_of_queue(Packet* packet) {throw cRuntimeError("insert_front_of_queue not implemented");};
    virtual cPacket* get_index(unsigned int idx) {throw cRuntimeError("get_index not implemented");};
};

} // namespace queueing
} // namespace inet

#endif // ifndef __INET_IPACKETQUEUE_H

