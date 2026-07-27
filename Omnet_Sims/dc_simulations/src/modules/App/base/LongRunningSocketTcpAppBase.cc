//
// Copyright (C) 2004 Andras Varga
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
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "LongRunningSocketTcpAppBase.h"

#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include "inet/applications/common/SocketTag_m.h"
#include "inet/transportlayer/tcp/Tcp.h"

using namespace inet;

#define INIT_PORT 80
#define MAX_PORT_NUM 65450      // 65535 - 85

simsignal_t LongRunningSocketTcpAppBase::connectSignal = registerSignal("connect");

LongRunningSocketTcpAppBase::~LongRunningSocketTcpAppBase() {
    for (auto socket_pair: socket_map.getMap())
        delete socket_pair.second;
}

void LongRunningSocketTcpAppBase::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        numSessions = numBroken = packetsSent = packetsRcvd = bytesSent = bytesRcvd = 0;

        WATCH(numSessions);
        WATCH(numBroken);
        WATCH(packetsSent);
        WATCH(packetsRcvd);
        WATCH(bytesSent);
        WATCH(bytesRcvd);


    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // parameters
//        const char *localAddress = par("localAddress");
//        int localPort = par("localPort");
//        socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
//
//        socket.setCallback(this);
//        socket.setOutputGate(gate("socketOut"));
//        int num_aggs = getAncestorPar("num_aggs").intValue();
//        int num_servers_under_each_agg = getAncestorPar("num_servers").intValue();
    }
}

void LongRunningSocketTcpAppBase::handleMessageWhenUp(cMessage *msg)
{
    EV << "SEPEHR: Handling Message LongRunningSocketTcpAppBase! msg is: " << msg->str() << endl;
    if (msg->isSelfMessage()) {
        EV << "SEPEHR: Calling handleTimer" << endl;
        handleTimer(msg);
    }

    else {
        EV << "SEPEHR: Calling socket.processMessage" << endl;

        TcpSocket *socket = check_and_cast_nullable<TcpSocket*>(socket_map.findSocketFor(msg));
        if (socket) {
            socket->processMessage(msg);
            return;
        }

        throw cRuntimeError("message %s(%s) arrived for unknown socket 1\n", msg->getFullName(), msg->getClassName());
        delete msg;
    }
}

void LongRunningSocketTcpAppBase::connect(int dest_server_idx, int connect_port)
{
    TcpSocket* socket;
//    bool found = false;
    bool is_server0 = getParentModule()->getIndex() == 0;
    for (auto socket_pair: socket_map.getMap()) {
        socket = check_and_cast_nullable<TcpSocket*>(socket_pair.second);
        if (!socket->isOpen()) {
            throw cRuntimeError("In long running, how is a socket in socket map but not open!!");
        }
    }


    std::string connect_address_str = "server[" + std::to_string(dest_server_idx) + "]";
    const char* connect_address = connect_address_str.c_str();
    L3Address destination;
    L3AddressResolver().tryResolve(connect_address, destination);

    std::string tcp_module_str = getParentModule()->getFullPath() + ".tcp";
    tcp::Tcp *tcp_module = check_and_cast<tcp::Tcp *>(getModuleByPath(tcp_module_str.c_str()));
    auto padding_for_dest_found = tcp_module->dest_to_padding_mapper.find(destination.str());
    int local_port_padding, local_port;
    if (padding_for_dest_found == tcp_module->dest_to_padding_mapper.end()) {
        local_port_padding = 1;
        tcp_module->dest_to_padding_mapper.insert(
                                    std::pair<std::string, int>(
                                            destination.str(), 2));
    } else {
        local_port_padding = padding_for_dest_found->second;
        if ((INIT_PORT + local_port_padding + 1) % MAX_PORT_NUM == 0)
            padding_for_dest_found->second = 1;
        else
            padding_for_dest_found->second++;
    }
    local_port = INIT_PORT + local_port_padding;

    socket = new TcpSocket();
    const char *localAddress = par("localAddress");
    socket->bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), local_port);
    socket->setCallback(this);
    socket->setOutputGate(gate("socketOut"));
    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket->setTimeToLive(timeToLive);

    int dscp = par("dscp");
    if (dscp != -1)
        socket->setDscp(dscp);

    // connect
    if (destination.isUnspecified()) {
        throw cRuntimeError("Connecting to %a port= %s: cannot resolve destination address\n", connect_address, connect_port);
    }
    else {
        EV_INFO << "Connecting to " << connect_address << "(" << destination << ") port=" << connect_port << endl;
        socket->connect(destination, connect_port);
        numSessions++;
//        emit(connectSignal, 1L);
    }
    if (is_server0)
        EV << "SEPEHR: adding socket with id: " << socket->getSocketId() <<
                " to the list for server " << dest_server_idx << ". Before adding the size of socket_map is "
                << socket_map.size();
    socket_map.addSocket(socket);
    if (is_server0)
        EV << ". after adding it's size is " << socket_map.size() << endl;

    socket_to_destination_mapper.insert(std::pair<TcpSocket*, unsigned long>(socket, dest_server_idx));
    destination_to_socket_mapper.insert(std::pair<unsigned long, TcpSocket*>(dest_server_idx, socket));

}

void LongRunningSocketTcpAppBase::socketEstablished(TcpSocket *)
{
    // *redefine* to perform or schedule first sending
    EV_INFO << "connected\n";
}

void LongRunningSocketTcpAppBase::close(int socket_id)
{
    EV_INFO << "issuing CLOSE command\n";

    TcpSocket *socket = check_and_cast_nullable<TcpSocket*>(socket_map.getSocketById(socket_id));
    if (socket) {
        socket->close();
        return;
    }

    throw cRuntimeError("No socket was found for socket id %d", socket_id);
}

void LongRunningSocketTcpAppBase::sendPacket(Packet *msg)
{
    TcpSocket *socket = check_and_cast_nullable<TcpSocket*>(socket_map.findSocketFor(msg));
    if (socket) {
        delete msg->removeTagIfPresent<SocketInd>();
        EV << "SEPEHR: the packet is related to a socket with id " << socket->getSocketId() << endl;
        int numBytes = msg->getByteLength();
        emit(packetSentSignal, msg);
        socket->send(msg);

        packetsSent++;
        bytesSent += numBytes;
        return;
    }

    throw cRuntimeError("message %s(%s) arrived for unknown socket 2\n", msg->getFullName(), msg->getClassName());    delete msg;

}

void LongRunningSocketTcpAppBase::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();
//    getDisplayString().setTagArg("t", 0, TcpSocket::stateName(socket.getState()));
}

void LongRunningSocketTcpAppBase::socketDataArrived(TcpSocket *, Packet *msg, bool)
{
    // *redefine* to perform or schedule next sending
    packetsRcvd++;
    bytesRcvd += msg->getByteLength();\
    EV << "Emitting packetReceivedSignal with msg: " << msg << endl;
    emit(packetReceivedSignal, msg);
    delete msg;
}

void LongRunningSocketTcpAppBase::socketPeerClosed(TcpSocket *socket_)
{
//    ASSERT(socket_ == &socket);
    // close the connection (if not already closed)
//    if (socket.getState() == TcpSocket::PEER_CLOSED) {
//        EV_INFO << "remote TCP closed, closing here as well\n";
//        close();
//    }
}

void LongRunningSocketTcpAppBase::socketClosed(TcpSocket *socket)
{
    // *redefine* to start another session etc.
    EV_INFO << "connection closed\n";
}

void LongRunningSocketTcpAppBase::socketFailure(TcpSocket *socket, int code)
{
    // subclasses may override this function, and add code try to reconnect after a delay.
    EV_WARN << "connection broken\n";
    numBroken++;

}

void LongRunningSocketTcpAppBase::finish()
{
    std::string modulePath = getFullPath();

    EV_INFO << modulePath << ": opened " << numSessions << " sessions\n";
    EV_INFO << modulePath << ": sent " << bytesSent << " bytes in " << packetsSent << " packets\n";
    EV_INFO << modulePath << ": received " << bytesRcvd << " bytes in " << packetsRcvd << " packets\n";
}

