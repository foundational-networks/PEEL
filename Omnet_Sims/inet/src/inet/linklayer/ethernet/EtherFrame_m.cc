//
// Generated file, do not edit! Created by nedtool 5.6 from inet/linklayer/ethernet/EtherFrame.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include "EtherFrame_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace {
template <class T> inline
typename std::enable_if<std::is_polymorphic<T>::value && std::is_base_of<omnetpp::cObject,T>::value, void *>::type
toVoidPtr(T* t)
{
    return (void *)(static_cast<const omnetpp::cObject *>(t));
}

template <class T> inline
typename std::enable_if<std::is_polymorphic<T>::value && !std::is_base_of<omnetpp::cObject,T>::value, void *>::type
toVoidPtr(T* t)
{
    return (void *)dynamic_cast<const void *>(t);
}

template <class T> inline
typename std::enable_if<!std::is_polymorphic<T>::value, void *>::type
toVoidPtr(T* t)
{
    return (void *)static_cast<const void *>(t);
}

}

namespace inet {

// forward
template<typename T, typename A>
std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec);

// Template rule to generate operator<< for shared_ptr<T>
template<typename T>
inline std::ostream& operator<<(std::ostream& out,const std::shared_ptr<T>& t) { return out << t.get(); }

// Template rule which fires if a struct or class doesn't have operator<<
template<typename T>
inline std::ostream& operator<<(std::ostream& out,const T&) {return out;}

// operator<< for std::vector<T>
template<typename T, typename A>
inline std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec)
{
    out.put('{');
    for(typename std::vector<T,A>::const_iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (it != vec.begin()) {
            out.put(','); out.put(' ');
        }
        out << *it;
    }
    out.put('}');

    char buf[32];
    sprintf(buf, " (size=%u)", (unsigned int)vec.size());
    out.write(buf, strlen(buf));
    return out;
}

EXECUTE_ON_STARTUP(
    omnetpp::cEnum *e = omnetpp::cEnum::find("inet::EthernetControlOpCode");
    if (!e) omnetpp::enums.getInstance()->add(e = new omnetpp::cEnum("inet::EthernetControlOpCode"));
    e->insert(ETHERNET_CONTROL_PAUSE, "ETHERNET_CONTROL_PAUSE");
)

Register_Class(EthernetMacHeader)

EthernetMacHeader::EthernetMacHeader() : ::inet::FieldsChunk()
{
    this->setChunkLength(B(ETHER_MAC_HEADER_BYTES));

}

EthernetMacHeader::EthernetMacHeader(const EthernetMacHeader& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

EthernetMacHeader::~EthernetMacHeader()
{
    delete this->cTag;
    delete this->sTag;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    delete [] this->dst_idx_list;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    delete [] this->rsbf_seed_list;
    delete [] this->rsbf_bitstream_list;
    delete [] this->partial_mcast_core_pkt;
    delete [] this->CIDR_agg_pkt;
    delete [] this->CIDR_core_pkt;
    delete [] this->input_ports_passed;
    delete [] this->affected_addresses;
}

EthernetMacHeader& EthernetMacHeader::operator=(const EthernetMacHeader& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void EthernetMacHeader::copy(const EthernetMacHeader& other)
{
    this->dest = other.dest;
    this->src = other.src;
    delete this->cTag;
    this->cTag = other.cTag;
    if (this->cTag != nullptr) {
        this->cTag = this->cTag->dup();
    }
    delete this->sTag;
    this->sTag = other.sTag;
    if (this->sTag != nullptr) {
        this->sTag = this->sTag->dup();
    }
    this->typeOrLength = other.typeOrLength;
    this->hop_count = other.hop_count;
    this->prio_queue_idx = other.prio_queue_idx;
    this->isFB_ = other.isFB_;
    this->allow_same_input_output = other.allow_same_input_output;
    this->original_interface_id = other.original_interface_id;
    this->bouncedDistance = other.bouncedDistance;
    this->maxBouncedDistance = other.maxBouncedDistance;
    this->bouncedHop = other.bouncedHop;
    this->totalHopNum = other.totalHopNum;
    this->is_bursty = other.is_bursty;
    this->payload_length = other.payload_length;
    this->total_length = other.total_length;
    this->offset = other.offset;
    this->is_v2_dropped_packet_header = other.is_v2_dropped_packet_header;
    this->queue_occupancy = other.queue_occupancy;
    this->time_packet_received_at_nic = other.time_packet_received_at_nic;
    this->local_nic_rx_delay = other.local_nic_rx_delay;
    this->remote_queueing_time = other.remote_queueing_time;
    this->fabric_delay_time_sent_from_source = other.fabric_delay_time_sent_from_source;
    this->time_packet_sent_from_src = other.time_packet_sent_from_src;
    this->is_deflected = other.is_deflected;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = (other.ports_to_dest_idx_arraysize==0) ? nullptr : new InnerList[other.ports_to_dest_idx_arraysize];
    ports_to_dest_idx_arraysize = other.ports_to_dest_idx_arraysize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++) {
        this->ports_to_dest_idx[i] = other.ports_to_dest_idx[i];
        this->ports_to_dest_idx[i].setName(other.ports_to_dest_idx[i].getName());
        take(&this->ports_to_dest_idx[i]);
    }
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = (other.jump_to_idx_arraysize==0) ? nullptr : new InnerList[other.jump_to_idx_arraysize];
    jump_to_idx_arraysize = other.jump_to_idx_arraysize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++) {
        this->jump_to_idx[i] = other.jump_to_idx[i];
        this->jump_to_idx[i].setName(other.jump_to_idx[i].getName());
        take(&this->jump_to_idx[i]);
    }
    delete [] this->dst_idx_list;
    this->dst_idx_list = (other.dst_idx_list_arraysize==0) ? nullptr : new int[other.dst_idx_list_arraysize];
    dst_idx_list_arraysize = other.dst_idx_list_arraysize;
    for (size_t i = 0; i < dst_idx_list_arraysize; i++) {
        this->dst_idx_list[i] = other.dst_idx_list[i];
    }
    this->start_from_idx = other.start_from_idx;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = (other.all_ports_to_dst_arraysize==0) ? nullptr : new TwoDInnerList[other.all_ports_to_dst_arraysize];
    all_ports_to_dst_arraysize = other.all_ports_to_dst_arraysize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++) {
        this->all_ports_to_dst[i] = other.all_ports_to_dst[i];
        this->all_ports_to_dst[i].setName(other.all_ports_to_dst[i].getName());
        take(&this->all_ports_to_dst[i]);
    }
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = (other.all_jump_to_idx_arraysize==0) ? nullptr : new TwoDInnerList[other.all_jump_to_idx_arraysize];
    all_jump_to_idx_arraysize = other.all_jump_to_idx_arraysize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++) {
        this->all_jump_to_idx[i] = other.all_jump_to_idx[i];
        this->all_jump_to_idx[i].setName(other.all_jump_to_idx[i].getName());
        take(&this->all_jump_to_idx[i]);
    }
    this->current_tree_idx = other.current_tree_idx;
    this->ina_leaf_aggregation_num = other.ina_leaf_aggregation_num;
    this->ina_spine_aggregation_num = other.ina_spine_aggregation_num;
    this->ina_core_aggregation_num = other.ina_core_aggregation_num;
    this->tree_direction = other.tree_direction;
    this->query_id = other.query_id;
    this->flow_id = other.flow_id;
    this->collective_scale = other.collective_scale;
    this->collective_alg = other.collective_alg;
    this->pfc_signal = other.pfc_signal;
    this->is_ecn_marked = other.is_ecn_marked;
    this->cnp_flow_id = other.cnp_flow_id;
    this->cnp_src_gpu_id = other.cnp_src_gpu_id;
    this->src_gpu_idx = other.src_gpu_idx;
    this->seq_num = other.seq_num;
    this->orca_pkt_seen_agent = other.orca_pkt_seen_agent;
    this->orca_agent_port_index = other.orca_agent_port_index;
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = (other.rsbf_seed_list_arraysize==0) ? nullptr : new unsigned int[other.rsbf_seed_list_arraysize];
    rsbf_seed_list_arraysize = other.rsbf_seed_list_arraysize;
    for (size_t i = 0; i < rsbf_seed_list_arraysize; i++) {
        this->rsbf_seed_list[i] = other.rsbf_seed_list[i];
    }
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = (other.rsbf_bitstream_list_arraysize==0) ? nullptr : new omnetpp::opp_string[other.rsbf_bitstream_list_arraysize];
    rsbf_bitstream_list_arraysize = other.rsbf_bitstream_list_arraysize;
    for (size_t i = 0; i < rsbf_bitstream_list_arraysize; i++) {
        this->rsbf_bitstream_list[i] = other.rsbf_bitstream_list[i];
    }
    this->rsbf_redundant_send = other.rsbf_redundant_send;
    this->dst_tor_idx = other.dst_tor_idx;
    delete [] this->partial_mcast_core_pkt;
    this->partial_mcast_core_pkt = (other.partial_mcast_core_pkt_arraysize==0) ? nullptr : new Packet *[other.partial_mcast_core_pkt_arraysize];
    partial_mcast_core_pkt_arraysize = other.partial_mcast_core_pkt_arraysize;
    for (size_t i = 0; i < partial_mcast_core_pkt_arraysize; i++) {
        this->partial_mcast_core_pkt[i] = other.partial_mcast_core_pkt[i];
    }
    this->multicast_pkt_in_programmable_core = other.multicast_pkt_in_programmable_core;
    this->agg_cidr_group_id = other.agg_cidr_group_id;
    this->agg_cidr_group_member_count = other.agg_cidr_group_member_count;
    this->is_redundant_cidr_pkt = other.is_redundant_cidr_pkt;
    delete [] this->CIDR_agg_pkt;
    this->CIDR_agg_pkt = (other.CIDR_agg_pkt_arraysize==0) ? nullptr : new Packet *[other.CIDR_agg_pkt_arraysize];
    CIDR_agg_pkt_arraysize = other.CIDR_agg_pkt_arraysize;
    for (size_t i = 0; i < CIDR_agg_pkt_arraysize; i++) {
        this->CIDR_agg_pkt[i] = other.CIDR_agg_pkt[i];
    }
    this->multicast_pkt_in_CIDR_agg = other.multicast_pkt_in_CIDR_agg;
    this->core_cidr_group_id = other.core_cidr_group_id;
    this->core_cidr_group_member_count = other.core_cidr_group_member_count;
    delete [] this->CIDR_core_pkt;
    this->CIDR_core_pkt = (other.CIDR_core_pkt_arraysize==0) ? nullptr : new Packet *[other.CIDR_core_pkt_arraysize];
    CIDR_core_pkt_arraysize = other.CIDR_core_pkt_arraysize;
    for (size_t i = 0; i < CIDR_core_pkt_arraysize; i++) {
        this->CIDR_core_pkt[i] = other.CIDR_core_pkt[i];
    }
    this->multicast_pkt_in_CIDR_core = other.multicast_pkt_in_CIDR_core;
    this->original_pkt = other.original_pkt;
    this->is_drop_notif = other.is_drop_notif;
    delete [] this->input_ports_passed;
    this->input_ports_passed = (other.input_ports_passed_arraysize==0) ? nullptr : new unsigned int[other.input_ports_passed_arraysize];
    input_ports_passed_arraysize = other.input_ports_passed_arraysize;
    for (size_t i = 0; i < input_ports_passed_arraysize; i++) {
        this->input_ports_passed[i] = other.input_ports_passed[i];
    }
    this->original_pkt_name = other.original_pkt_name;
    this->recovery_time = other.recovery_time;
    delete [] this->affected_addresses;
    this->affected_addresses = (other.affected_addresses_arraysize==0) ? nullptr : new MacAddress[other.affected_addresses_arraysize];
    affected_addresses_arraysize = other.affected_addresses_arraysize;
    for (size_t i = 0; i < affected_addresses_arraysize; i++) {
        this->affected_addresses[i] = other.affected_addresses[i];
    }
    this->swift_send_time = other.swift_send_time;
    this->cidr_base_flow_id = other.cidr_base_flow_id;
    this->cidr_base_src_gpu_idx = other.cidr_base_src_gpu_idx;
    this->in_src_sharding = other.in_src_sharding;
    this->using_orca = other.using_orca;
    this->using_elmo = other.using_elmo;
}

void EthernetMacHeader::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->dest);
    doParsimPacking(b,this->src);
    doParsimPacking(b,this->cTag);
    doParsimPacking(b,this->sTag);
    doParsimPacking(b,this->typeOrLength);
    doParsimPacking(b,this->hop_count);
    doParsimPacking(b,this->prio_queue_idx);
    doParsimPacking(b,this->isFB_);
    doParsimPacking(b,this->allow_same_input_output);
    doParsimPacking(b,this->original_interface_id);
    doParsimPacking(b,this->bouncedDistance);
    doParsimPacking(b,this->maxBouncedDistance);
    doParsimPacking(b,this->bouncedHop);
    doParsimPacking(b,this->totalHopNum);
    doParsimPacking(b,this->is_bursty);
    doParsimPacking(b,this->payload_length);
    doParsimPacking(b,this->total_length);
    doParsimPacking(b,this->offset);
    doParsimPacking(b,this->is_v2_dropped_packet_header);
    doParsimPacking(b,this->queue_occupancy);
    doParsimPacking(b,this->time_packet_received_at_nic);
    doParsimPacking(b,this->local_nic_rx_delay);
    doParsimPacking(b,this->remote_queueing_time);
    doParsimPacking(b,this->fabric_delay_time_sent_from_source);
    doParsimPacking(b,this->time_packet_sent_from_src);
    doParsimPacking(b,this->is_deflected);
    b->pack(ports_to_dest_idx_arraysize);
    doParsimArrayPacking(b,this->ports_to_dest_idx,ports_to_dest_idx_arraysize);
    b->pack(jump_to_idx_arraysize);
    doParsimArrayPacking(b,this->jump_to_idx,jump_to_idx_arraysize);
    b->pack(dst_idx_list_arraysize);
    doParsimArrayPacking(b,this->dst_idx_list,dst_idx_list_arraysize);
    doParsimPacking(b,this->start_from_idx);
    b->pack(all_ports_to_dst_arraysize);
    doParsimArrayPacking(b,this->all_ports_to_dst,all_ports_to_dst_arraysize);
    b->pack(all_jump_to_idx_arraysize);
    doParsimArrayPacking(b,this->all_jump_to_idx,all_jump_to_idx_arraysize);
    doParsimPacking(b,this->current_tree_idx);
    doParsimPacking(b,this->ina_leaf_aggregation_num);
    doParsimPacking(b,this->ina_spine_aggregation_num);
    doParsimPacking(b,this->ina_core_aggregation_num);
    doParsimPacking(b,this->tree_direction);
    doParsimPacking(b,this->query_id);
    doParsimPacking(b,this->flow_id);
    doParsimPacking(b,this->collective_scale);
    doParsimPacking(b,this->collective_alg);
    doParsimPacking(b,this->pfc_signal);
    doParsimPacking(b,this->is_ecn_marked);
    doParsimPacking(b,this->cnp_flow_id);
    doParsimPacking(b,this->cnp_src_gpu_id);
    doParsimPacking(b,this->src_gpu_idx);
    doParsimPacking(b,this->seq_num);
    doParsimPacking(b,this->orca_pkt_seen_agent);
    doParsimPacking(b,this->orca_agent_port_index);
    b->pack(rsbf_seed_list_arraysize);
    doParsimArrayPacking(b,this->rsbf_seed_list,rsbf_seed_list_arraysize);
    b->pack(rsbf_bitstream_list_arraysize);
    doParsimArrayPacking(b,this->rsbf_bitstream_list,rsbf_bitstream_list_arraysize);
    doParsimPacking(b,this->rsbf_redundant_send);
    doParsimPacking(b,this->dst_tor_idx);
    b->pack(partial_mcast_core_pkt_arraysize);
    doParsimArrayPacking(b,this->partial_mcast_core_pkt,partial_mcast_core_pkt_arraysize);
    doParsimPacking(b,this->multicast_pkt_in_programmable_core);
    doParsimPacking(b,this->agg_cidr_group_id);
    doParsimPacking(b,this->agg_cidr_group_member_count);
    doParsimPacking(b,this->is_redundant_cidr_pkt);
    b->pack(CIDR_agg_pkt_arraysize);
    doParsimArrayPacking(b,this->CIDR_agg_pkt,CIDR_agg_pkt_arraysize);
    doParsimPacking(b,this->multicast_pkt_in_CIDR_agg);
    doParsimPacking(b,this->core_cidr_group_id);
    doParsimPacking(b,this->core_cidr_group_member_count);
    b->pack(CIDR_core_pkt_arraysize);
    doParsimArrayPacking(b,this->CIDR_core_pkt,CIDR_core_pkt_arraysize);
    doParsimPacking(b,this->multicast_pkt_in_CIDR_core);
    doParsimPacking(b,this->original_pkt);
    doParsimPacking(b,this->is_drop_notif);
    b->pack(input_ports_passed_arraysize);
    doParsimArrayPacking(b,this->input_ports_passed,input_ports_passed_arraysize);
    doParsimPacking(b,this->original_pkt_name);
    doParsimPacking(b,this->recovery_time);
    b->pack(affected_addresses_arraysize);
    doParsimArrayPacking(b,this->affected_addresses,affected_addresses_arraysize);
    doParsimPacking(b,this->swift_send_time);
    doParsimPacking(b,this->cidr_base_flow_id);
    doParsimPacking(b,this->cidr_base_src_gpu_idx);
    doParsimPacking(b,this->in_src_sharding);
    doParsimPacking(b,this->using_orca);
    doParsimPacking(b,this->using_elmo);
}

void EthernetMacHeader::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->dest);
    doParsimUnpacking(b,this->src);
    doParsimUnpacking(b,this->cTag);
    doParsimUnpacking(b,this->sTag);
    doParsimUnpacking(b,this->typeOrLength);
    doParsimUnpacking(b,this->hop_count);
    doParsimUnpacking(b,this->prio_queue_idx);
    doParsimUnpacking(b,this->isFB_);
    doParsimUnpacking(b,this->allow_same_input_output);
    doParsimUnpacking(b,this->original_interface_id);
    doParsimUnpacking(b,this->bouncedDistance);
    doParsimUnpacking(b,this->maxBouncedDistance);
    doParsimUnpacking(b,this->bouncedHop);
    doParsimUnpacking(b,this->totalHopNum);
    doParsimUnpacking(b,this->is_bursty);
    doParsimUnpacking(b,this->payload_length);
    doParsimUnpacking(b,this->total_length);
    doParsimUnpacking(b,this->offset);
    doParsimUnpacking(b,this->is_v2_dropped_packet_header);
    doParsimUnpacking(b,this->queue_occupancy);
    doParsimUnpacking(b,this->time_packet_received_at_nic);
    doParsimUnpacking(b,this->local_nic_rx_delay);
    doParsimUnpacking(b,this->remote_queueing_time);
    doParsimUnpacking(b,this->fabric_delay_time_sent_from_source);
    doParsimUnpacking(b,this->time_packet_sent_from_src);
    doParsimUnpacking(b,this->is_deflected);
    delete [] this->ports_to_dest_idx;
    b->unpack(ports_to_dest_idx_arraysize);
    if (ports_to_dest_idx_arraysize == 0) {
        this->ports_to_dest_idx = nullptr;
    } else {
        this->ports_to_dest_idx = new InnerList[ports_to_dest_idx_arraysize];
        doParsimArrayUnpacking(b,this->ports_to_dest_idx,ports_to_dest_idx_arraysize);
    }
    delete [] this->jump_to_idx;
    b->unpack(jump_to_idx_arraysize);
    if (jump_to_idx_arraysize == 0) {
        this->jump_to_idx = nullptr;
    } else {
        this->jump_to_idx = new InnerList[jump_to_idx_arraysize];
        doParsimArrayUnpacking(b,this->jump_to_idx,jump_to_idx_arraysize);
    }
    delete [] this->dst_idx_list;
    b->unpack(dst_idx_list_arraysize);
    if (dst_idx_list_arraysize == 0) {
        this->dst_idx_list = nullptr;
    } else {
        this->dst_idx_list = new int[dst_idx_list_arraysize];
        doParsimArrayUnpacking(b,this->dst_idx_list,dst_idx_list_arraysize);
    }
    doParsimUnpacking(b,this->start_from_idx);
    delete [] this->all_ports_to_dst;
    b->unpack(all_ports_to_dst_arraysize);
    if (all_ports_to_dst_arraysize == 0) {
        this->all_ports_to_dst = nullptr;
    } else {
        this->all_ports_to_dst = new TwoDInnerList[all_ports_to_dst_arraysize];
        doParsimArrayUnpacking(b,this->all_ports_to_dst,all_ports_to_dst_arraysize);
    }
    delete [] this->all_jump_to_idx;
    b->unpack(all_jump_to_idx_arraysize);
    if (all_jump_to_idx_arraysize == 0) {
        this->all_jump_to_idx = nullptr;
    } else {
        this->all_jump_to_idx = new TwoDInnerList[all_jump_to_idx_arraysize];
        doParsimArrayUnpacking(b,this->all_jump_to_idx,all_jump_to_idx_arraysize);
    }
    doParsimUnpacking(b,this->current_tree_idx);
    doParsimUnpacking(b,this->ina_leaf_aggregation_num);
    doParsimUnpacking(b,this->ina_spine_aggregation_num);
    doParsimUnpacking(b,this->ina_core_aggregation_num);
    doParsimUnpacking(b,this->tree_direction);
    doParsimUnpacking(b,this->query_id);
    doParsimUnpacking(b,this->flow_id);
    doParsimUnpacking(b,this->collective_scale);
    doParsimUnpacking(b,this->collective_alg);
    doParsimUnpacking(b,this->pfc_signal);
    doParsimUnpacking(b,this->is_ecn_marked);
    doParsimUnpacking(b,this->cnp_flow_id);
    doParsimUnpacking(b,this->cnp_src_gpu_id);
    doParsimUnpacking(b,this->src_gpu_idx);
    doParsimUnpacking(b,this->seq_num);
    doParsimUnpacking(b,this->orca_pkt_seen_agent);
    doParsimUnpacking(b,this->orca_agent_port_index);
    delete [] this->rsbf_seed_list;
    b->unpack(rsbf_seed_list_arraysize);
    if (rsbf_seed_list_arraysize == 0) {
        this->rsbf_seed_list = nullptr;
    } else {
        this->rsbf_seed_list = new unsigned int[rsbf_seed_list_arraysize];
        doParsimArrayUnpacking(b,this->rsbf_seed_list,rsbf_seed_list_arraysize);
    }
    delete [] this->rsbf_bitstream_list;
    b->unpack(rsbf_bitstream_list_arraysize);
    if (rsbf_bitstream_list_arraysize == 0) {
        this->rsbf_bitstream_list = nullptr;
    } else {
        this->rsbf_bitstream_list = new omnetpp::opp_string[rsbf_bitstream_list_arraysize];
        doParsimArrayUnpacking(b,this->rsbf_bitstream_list,rsbf_bitstream_list_arraysize);
    }
    doParsimUnpacking(b,this->rsbf_redundant_send);
    doParsimUnpacking(b,this->dst_tor_idx);
    delete [] this->partial_mcast_core_pkt;
    b->unpack(partial_mcast_core_pkt_arraysize);
    if (partial_mcast_core_pkt_arraysize == 0) {
        this->partial_mcast_core_pkt = nullptr;
    } else {
        this->partial_mcast_core_pkt = new Packet *[partial_mcast_core_pkt_arraysize];
        doParsimArrayUnpacking(b,this->partial_mcast_core_pkt,partial_mcast_core_pkt_arraysize);
    }
    doParsimUnpacking(b,this->multicast_pkt_in_programmable_core);
    doParsimUnpacking(b,this->agg_cidr_group_id);
    doParsimUnpacking(b,this->agg_cidr_group_member_count);
    doParsimUnpacking(b,this->is_redundant_cidr_pkt);
    delete [] this->CIDR_agg_pkt;
    b->unpack(CIDR_agg_pkt_arraysize);
    if (CIDR_agg_pkt_arraysize == 0) {
        this->CIDR_agg_pkt = nullptr;
    } else {
        this->CIDR_agg_pkt = new Packet *[CIDR_agg_pkt_arraysize];
        doParsimArrayUnpacking(b,this->CIDR_agg_pkt,CIDR_agg_pkt_arraysize);
    }
    doParsimUnpacking(b,this->multicast_pkt_in_CIDR_agg);
    doParsimUnpacking(b,this->core_cidr_group_id);
    doParsimUnpacking(b,this->core_cidr_group_member_count);
    delete [] this->CIDR_core_pkt;
    b->unpack(CIDR_core_pkt_arraysize);
    if (CIDR_core_pkt_arraysize == 0) {
        this->CIDR_core_pkt = nullptr;
    } else {
        this->CIDR_core_pkt = new Packet *[CIDR_core_pkt_arraysize];
        doParsimArrayUnpacking(b,this->CIDR_core_pkt,CIDR_core_pkt_arraysize);
    }
    doParsimUnpacking(b,this->multicast_pkt_in_CIDR_core);
    doParsimUnpacking(b,this->original_pkt);
    doParsimUnpacking(b,this->is_drop_notif);
    delete [] this->input_ports_passed;
    b->unpack(input_ports_passed_arraysize);
    if (input_ports_passed_arraysize == 0) {
        this->input_ports_passed = nullptr;
    } else {
        this->input_ports_passed = new unsigned int[input_ports_passed_arraysize];
        doParsimArrayUnpacking(b,this->input_ports_passed,input_ports_passed_arraysize);
    }
    doParsimUnpacking(b,this->original_pkt_name);
    doParsimUnpacking(b,this->recovery_time);
    delete [] this->affected_addresses;
    b->unpack(affected_addresses_arraysize);
    if (affected_addresses_arraysize == 0) {
        this->affected_addresses = nullptr;
    } else {
        this->affected_addresses = new MacAddress[affected_addresses_arraysize];
        doParsimArrayUnpacking(b,this->affected_addresses,affected_addresses_arraysize);
    }
    doParsimUnpacking(b,this->swift_send_time);
    doParsimUnpacking(b,this->cidr_base_flow_id);
    doParsimUnpacking(b,this->cidr_base_src_gpu_idx);
    doParsimUnpacking(b,this->in_src_sharding);
    doParsimUnpacking(b,this->using_orca);
    doParsimUnpacking(b,this->using_elmo);
}

const MacAddress& EthernetMacHeader::getDest() const
{
    return this->dest;
}

void EthernetMacHeader::setDest(const MacAddress& dest)
{
    handleChange();
    this->dest = dest;
}

const MacAddress& EthernetMacHeader::getSrc() const
{
    return this->src;
}

void EthernetMacHeader::setSrc(const MacAddress& src)
{
    handleChange();
    this->src = src;
}

const Ieee8021qHeader * EthernetMacHeader::getCTag() const
{
    return this->cTag;
}

void EthernetMacHeader::setCTag(Ieee8021qHeader * cTag)
{
    handleChange();
    if (this->cTag != nullptr) throw omnetpp::cRuntimeError("setCTag(): a value is already set, remove it first with dropCTag()");
    this->cTag = cTag;
}

Ieee8021qHeader * EthernetMacHeader::dropCTag()
{
    handleChange();
    Ieee8021qHeader * retval = this->cTag;
    this->cTag = nullptr;
    return retval;
}

const Ieee8021qHeader * EthernetMacHeader::getSTag() const
{
    return this->sTag;
}

void EthernetMacHeader::setSTag(Ieee8021qHeader * sTag)
{
    handleChange();
    if (this->sTag != nullptr) throw omnetpp::cRuntimeError("setSTag(): a value is already set, remove it first with dropSTag()");
    this->sTag = sTag;
}

Ieee8021qHeader * EthernetMacHeader::dropSTag()
{
    handleChange();
    Ieee8021qHeader * retval = this->sTag;
    this->sTag = nullptr;
    return retval;
}

int EthernetMacHeader::getTypeOrLength() const
{
    return this->typeOrLength;
}

void EthernetMacHeader::setTypeOrLength(int typeOrLength)
{
    handleChange();
    this->typeOrLength = typeOrLength;
}

int EthernetMacHeader::getHop_count() const
{
    return this->hop_count;
}

void EthernetMacHeader::setHop_count(int hop_count)
{
    handleChange();
    this->hop_count = hop_count;
}

int EthernetMacHeader::getPrio_queue_idx() const
{
    return this->prio_queue_idx;
}

void EthernetMacHeader::setPrio_queue_idx(int prio_queue_idx)
{
    handleChange();
    this->prio_queue_idx = prio_queue_idx;
}

bool EthernetMacHeader::isFB() const
{
    return this->isFB_;
}

void EthernetMacHeader::setIsFB(bool isFB)
{
    handleChange();
    this->isFB_ = isFB;
}

bool EthernetMacHeader::getAllow_same_input_output() const
{
    return this->allow_same_input_output;
}

void EthernetMacHeader::setAllow_same_input_output(bool allow_same_input_output)
{
    handleChange();
    this->allow_same_input_output = allow_same_input_output;
}

int EthernetMacHeader::getOriginal_interface_id() const
{
    return this->original_interface_id;
}

void EthernetMacHeader::setOriginal_interface_id(int original_interface_id)
{
    handleChange();
    this->original_interface_id = original_interface_id;
}

int EthernetMacHeader::getBouncedDistance() const
{
    return this->bouncedDistance;
}

void EthernetMacHeader::setBouncedDistance(int bouncedDistance)
{
    handleChange();
    this->bouncedDistance = bouncedDistance;
}

int EthernetMacHeader::getMaxBouncedDistance() const
{
    return this->maxBouncedDistance;
}

void EthernetMacHeader::setMaxBouncedDistance(int maxBouncedDistance)
{
    handleChange();
    this->maxBouncedDistance = maxBouncedDistance;
}

int EthernetMacHeader::getBouncedHop() const
{
    return this->bouncedHop;
}

void EthernetMacHeader::setBouncedHop(int bouncedHop)
{
    handleChange();
    this->bouncedHop = bouncedHop;
}

int EthernetMacHeader::getTotalHopNum() const
{
    return this->totalHopNum;
}

void EthernetMacHeader::setTotalHopNum(int totalHopNum)
{
    handleChange();
    this->totalHopNum = totalHopNum;
}

bool EthernetMacHeader::getIs_bursty() const
{
    return this->is_bursty;
}

void EthernetMacHeader::setIs_bursty(bool is_bursty)
{
    handleChange();
    this->is_bursty = is_bursty;
}

b EthernetMacHeader::getPayload_length() const
{
    return this->payload_length;
}

void EthernetMacHeader::setPayload_length(b payload_length)
{
    handleChange();
    this->payload_length = payload_length;
}

b EthernetMacHeader::getTotal_length() const
{
    return this->total_length;
}

void EthernetMacHeader::setTotal_length(b total_length)
{
    handleChange();
    this->total_length = total_length;
}

b EthernetMacHeader::getOffset() const
{
    return this->offset;
}

void EthernetMacHeader::setOffset(b offset)
{
    handleChange();
    this->offset = offset;
}

bool EthernetMacHeader::getIs_v2_dropped_packet_header() const
{
    return this->is_v2_dropped_packet_header;
}

void EthernetMacHeader::setIs_v2_dropped_packet_header(bool is_v2_dropped_packet_header)
{
    handleChange();
    this->is_v2_dropped_packet_header = is_v2_dropped_packet_header;
}

uint16_t EthernetMacHeader::getQueue_occupancy() const
{
    return this->queue_occupancy;
}

void EthernetMacHeader::setQueue_occupancy(uint16_t queue_occupancy)
{
    handleChange();
    this->queue_occupancy = queue_occupancy;
}

omnetpp::simtime_t EthernetMacHeader::getTime_packet_received_at_nic() const
{
    return this->time_packet_received_at_nic;
}

void EthernetMacHeader::setTime_packet_received_at_nic(omnetpp::simtime_t time_packet_received_at_nic)
{
    handleChange();
    this->time_packet_received_at_nic = time_packet_received_at_nic;
}

omnetpp::simtime_t EthernetMacHeader::getLocal_nic_rx_delay() const
{
    return this->local_nic_rx_delay;
}

void EthernetMacHeader::setLocal_nic_rx_delay(omnetpp::simtime_t local_nic_rx_delay)
{
    handleChange();
    this->local_nic_rx_delay = local_nic_rx_delay;
}

omnetpp::simtime_t EthernetMacHeader::getRemote_queueing_time() const
{
    return this->remote_queueing_time;
}

void EthernetMacHeader::setRemote_queueing_time(omnetpp::simtime_t remote_queueing_time)
{
    handleChange();
    this->remote_queueing_time = remote_queueing_time;
}

omnetpp::simtime_t EthernetMacHeader::getFabric_delay_time_sent_from_source() const
{
    return this->fabric_delay_time_sent_from_source;
}

void EthernetMacHeader::setFabric_delay_time_sent_from_source(omnetpp::simtime_t fabric_delay_time_sent_from_source)
{
    handleChange();
    this->fabric_delay_time_sent_from_source = fabric_delay_time_sent_from_source;
}

omnetpp::simtime_t EthernetMacHeader::getTime_packet_sent_from_src() const
{
    return this->time_packet_sent_from_src;
}

void EthernetMacHeader::setTime_packet_sent_from_src(omnetpp::simtime_t time_packet_sent_from_src)
{
    handleChange();
    this->time_packet_sent_from_src = time_packet_sent_from_src;
}

bool EthernetMacHeader::getIs_deflected() const
{
    return this->is_deflected;
}

void EthernetMacHeader::setIs_deflected(bool is_deflected)
{
    handleChange();
    this->is_deflected = is_deflected;
}

size_t EthernetMacHeader::getPorts_to_dest_idxArraySize() const
{
    return ports_to_dest_idx_arraysize;
}

const InnerList& EthernetMacHeader::getPorts_to_dest_idx(size_t k) const
{
    if (k >= ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size ports_to_dest_idx_arraysize indexed by %lu", (unsigned long)k);
    return this->ports_to_dest_idx[k];
}

void EthernetMacHeader::setPorts_to_dest_idxArraySize(size_t newSize)
{
    handleChange();
    InnerList *ports_to_dest_idx2 = (newSize==0) ? nullptr : new InnerList[newSize];
    size_t minSize = ports_to_dest_idx_arraysize < newSize ? ports_to_dest_idx_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i];
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = ports_to_dest_idx2;
    ports_to_dest_idx_arraysize = newSize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        take(&this->ports_to_dest_idx[i]);
}

void EthernetMacHeader::setPorts_to_dest_idx(size_t k, const InnerList& ports_to_dest_idx)
{
    if (k >= ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->ports_to_dest_idx[k] = ports_to_dest_idx;
}

void EthernetMacHeader::insertPorts_to_dest_idx(size_t k, const InnerList& ports_to_dest_idx)
{
    handleChange();
    if (k > ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = ports_to_dest_idx_arraysize + 1;
    InnerList *ports_to_dest_idx2 = new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i];
    ports_to_dest_idx2[k] = ports_to_dest_idx;
    for (i = k + 1; i < newSize; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i-1];
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = ports_to_dest_idx2;
    ports_to_dest_idx_arraysize = newSize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        take(&this->ports_to_dest_idx[i]);
}

void EthernetMacHeader::insertPorts_to_dest_idx(const InnerList& ports_to_dest_idx)
{
    insertPorts_to_dest_idx(ports_to_dest_idx_arraysize, ports_to_dest_idx);
}

void EthernetMacHeader::erasePorts_to_dest_idx(size_t k)
{
    if (k >= ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = ports_to_dest_idx_arraysize - 1;
    InnerList *ports_to_dest_idx2 = (newSize == 0) ? nullptr : new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i];
    for (i = k; i < newSize; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i+1];
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = ports_to_dest_idx2;
    ports_to_dest_idx_arraysize = newSize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        take(&this->ports_to_dest_idx[i]);
}

size_t EthernetMacHeader::getJump_to_idxArraySize() const
{
    return jump_to_idx_arraysize;
}

const InnerList& EthernetMacHeader::getJump_to_idx(size_t k) const
{
    if (k >= jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size jump_to_idx_arraysize indexed by %lu", (unsigned long)k);
    return this->jump_to_idx[k];
}

void EthernetMacHeader::setJump_to_idxArraySize(size_t newSize)
{
    handleChange();
    InnerList *jump_to_idx2 = (newSize==0) ? nullptr : new InnerList[newSize];
    size_t minSize = jump_to_idx_arraysize < newSize ? jump_to_idx_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        jump_to_idx2[i] = this->jump_to_idx[i];
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = jump_to_idx2;
    jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        take(&this->jump_to_idx[i]);
}

void EthernetMacHeader::setJump_to_idx(size_t k, const InnerList& jump_to_idx)
{
    if (k >= jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->jump_to_idx[k] = jump_to_idx;
}

void EthernetMacHeader::insertJump_to_idx(size_t k, const InnerList& jump_to_idx)
{
    handleChange();
    if (k > jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = jump_to_idx_arraysize + 1;
    InnerList *jump_to_idx2 = new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        jump_to_idx2[i] = this->jump_to_idx[i];
    jump_to_idx2[k] = jump_to_idx;
    for (i = k + 1; i < newSize; i++)
        jump_to_idx2[i] = this->jump_to_idx[i-1];
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = jump_to_idx2;
    jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        take(&this->jump_to_idx[i]);
}

void EthernetMacHeader::insertJump_to_idx(const InnerList& jump_to_idx)
{
    insertJump_to_idx(jump_to_idx_arraysize, jump_to_idx);
}

void EthernetMacHeader::eraseJump_to_idx(size_t k)
{
    if (k >= jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = jump_to_idx_arraysize - 1;
    InnerList *jump_to_idx2 = (newSize == 0) ? nullptr : new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        jump_to_idx2[i] = this->jump_to_idx[i];
    for (i = k; i < newSize; i++)
        jump_to_idx2[i] = this->jump_to_idx[i+1];
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = jump_to_idx2;
    jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        take(&this->jump_to_idx[i]);
}

size_t EthernetMacHeader::getDst_idx_listArraySize() const
{
    return dst_idx_list_arraysize;
}

int EthernetMacHeader::getDst_idx_list(size_t k) const
{
    if (k >= dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size dst_idx_list_arraysize indexed by %lu", (unsigned long)k);
    return this->dst_idx_list[k];
}

void EthernetMacHeader::setDst_idx_listArraySize(size_t newSize)
{
    handleChange();
    int *dst_idx_list2 = (newSize==0) ? nullptr : new int[newSize];
    size_t minSize = dst_idx_list_arraysize < newSize ? dst_idx_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        dst_idx_list2[i] = this->dst_idx_list[i];
    for (size_t i = minSize; i < newSize; i++)
        dst_idx_list2[i] = 0;
    delete [] this->dst_idx_list;
    this->dst_idx_list = dst_idx_list2;
    dst_idx_list_arraysize = newSize;
}

void EthernetMacHeader::setDst_idx_list(size_t k, int dst_idx_list)
{
    if (k >= dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->dst_idx_list[k] = dst_idx_list;
}

void EthernetMacHeader::insertDst_idx_list(size_t k, int dst_idx_list)
{
    handleChange();
    if (k > dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = dst_idx_list_arraysize + 1;
    int *dst_idx_list2 = new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        dst_idx_list2[i] = this->dst_idx_list[i];
    dst_idx_list2[k] = dst_idx_list;
    for (i = k + 1; i < newSize; i++)
        dst_idx_list2[i] = this->dst_idx_list[i-1];
    delete [] this->dst_idx_list;
    this->dst_idx_list = dst_idx_list2;
    dst_idx_list_arraysize = newSize;
}

void EthernetMacHeader::insertDst_idx_list(int dst_idx_list)
{
    insertDst_idx_list(dst_idx_list_arraysize, dst_idx_list);
}

void EthernetMacHeader::eraseDst_idx_list(size_t k)
{
    if (k >= dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = dst_idx_list_arraysize - 1;
    int *dst_idx_list2 = (newSize == 0) ? nullptr : new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        dst_idx_list2[i] = this->dst_idx_list[i];
    for (i = k; i < newSize; i++)
        dst_idx_list2[i] = this->dst_idx_list[i+1];
    delete [] this->dst_idx_list;
    this->dst_idx_list = dst_idx_list2;
    dst_idx_list_arraysize = newSize;
}

unsigned int EthernetMacHeader::getStart_from_idx() const
{
    return this->start_from_idx;
}

void EthernetMacHeader::setStart_from_idx(unsigned int start_from_idx)
{
    handleChange();
    this->start_from_idx = start_from_idx;
}

size_t EthernetMacHeader::getAll_ports_to_dstArraySize() const
{
    return all_ports_to_dst_arraysize;
}

const TwoDInnerList& EthernetMacHeader::getAll_ports_to_dst(size_t k) const
{
    if (k >= all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size all_ports_to_dst_arraysize indexed by %lu", (unsigned long)k);
    return this->all_ports_to_dst[k];
}

void EthernetMacHeader::setAll_ports_to_dstArraySize(size_t newSize)
{
    handleChange();
    TwoDInnerList *all_ports_to_dst2 = (newSize==0) ? nullptr : new TwoDInnerList[newSize];
    size_t minSize = all_ports_to_dst_arraysize < newSize ? all_ports_to_dst_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i];
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = all_ports_to_dst2;
    all_ports_to_dst_arraysize = newSize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        take(&this->all_ports_to_dst[i]);
}

void EthernetMacHeader::setAll_ports_to_dst(size_t k, const TwoDInnerList& all_ports_to_dst)
{
    if (k >= all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->all_ports_to_dst[k] = all_ports_to_dst;
}

void EthernetMacHeader::insertAll_ports_to_dst(size_t k, const TwoDInnerList& all_ports_to_dst)
{
    handleChange();
    if (k > all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = all_ports_to_dst_arraysize + 1;
    TwoDInnerList *all_ports_to_dst2 = new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i];
    all_ports_to_dst2[k] = all_ports_to_dst;
    for (i = k + 1; i < newSize; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i-1];
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = all_ports_to_dst2;
    all_ports_to_dst_arraysize = newSize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        take(&this->all_ports_to_dst[i]);
}

void EthernetMacHeader::insertAll_ports_to_dst(const TwoDInnerList& all_ports_to_dst)
{
    insertAll_ports_to_dst(all_ports_to_dst_arraysize, all_ports_to_dst);
}

void EthernetMacHeader::eraseAll_ports_to_dst(size_t k)
{
    if (k >= all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = all_ports_to_dst_arraysize - 1;
    TwoDInnerList *all_ports_to_dst2 = (newSize == 0) ? nullptr : new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i];
    for (i = k; i < newSize; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i+1];
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = all_ports_to_dst2;
    all_ports_to_dst_arraysize = newSize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        take(&this->all_ports_to_dst[i]);
}

size_t EthernetMacHeader::getAll_jump_to_idxArraySize() const
{
    return all_jump_to_idx_arraysize;
}

const TwoDInnerList& EthernetMacHeader::getAll_jump_to_idx(size_t k) const
{
    if (k >= all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size all_jump_to_idx_arraysize indexed by %lu", (unsigned long)k);
    return this->all_jump_to_idx[k];
}

void EthernetMacHeader::setAll_jump_to_idxArraySize(size_t newSize)
{
    handleChange();
    TwoDInnerList *all_jump_to_idx2 = (newSize==0) ? nullptr : new TwoDInnerList[newSize];
    size_t minSize = all_jump_to_idx_arraysize < newSize ? all_jump_to_idx_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i];
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = all_jump_to_idx2;
    all_jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        take(&this->all_jump_to_idx[i]);
}

void EthernetMacHeader::setAll_jump_to_idx(size_t k, const TwoDInnerList& all_jump_to_idx)
{
    if (k >= all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->all_jump_to_idx[k] = all_jump_to_idx;
}

void EthernetMacHeader::insertAll_jump_to_idx(size_t k, const TwoDInnerList& all_jump_to_idx)
{
    handleChange();
    if (k > all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = all_jump_to_idx_arraysize + 1;
    TwoDInnerList *all_jump_to_idx2 = new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i];
    all_jump_to_idx2[k] = all_jump_to_idx;
    for (i = k + 1; i < newSize; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i-1];
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = all_jump_to_idx2;
    all_jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        take(&this->all_jump_to_idx[i]);
}

void EthernetMacHeader::insertAll_jump_to_idx(const TwoDInnerList& all_jump_to_idx)
{
    insertAll_jump_to_idx(all_jump_to_idx_arraysize, all_jump_to_idx);
}

void EthernetMacHeader::eraseAll_jump_to_idx(size_t k)
{
    if (k >= all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = all_jump_to_idx_arraysize - 1;
    TwoDInnerList *all_jump_to_idx2 = (newSize == 0) ? nullptr : new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i];
    for (i = k; i < newSize; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i+1];
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = all_jump_to_idx2;
    all_jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        take(&this->all_jump_to_idx[i]);
}

int EthernetMacHeader::getCurrent_tree_idx() const
{
    return this->current_tree_idx;
}

void EthernetMacHeader::setCurrent_tree_idx(int current_tree_idx)
{
    handleChange();
    this->current_tree_idx = current_tree_idx;
}

int EthernetMacHeader::getIna_leaf_aggregation_num() const
{
    return this->ina_leaf_aggregation_num;
}

void EthernetMacHeader::setIna_leaf_aggregation_num(int ina_leaf_aggregation_num)
{
    handleChange();
    this->ina_leaf_aggregation_num = ina_leaf_aggregation_num;
}

int EthernetMacHeader::getIna_spine_aggregation_num() const
{
    return this->ina_spine_aggregation_num;
}

void EthernetMacHeader::setIna_spine_aggregation_num(int ina_spine_aggregation_num)
{
    handleChange();
    this->ina_spine_aggregation_num = ina_spine_aggregation_num;
}

int EthernetMacHeader::getIna_core_aggregation_num() const
{
    return this->ina_core_aggregation_num;
}

void EthernetMacHeader::setIna_core_aggregation_num(int ina_core_aggregation_num)
{
    handleChange();
    this->ina_core_aggregation_num = ina_core_aggregation_num;
}

int EthernetMacHeader::getTree_direction() const
{
    return this->tree_direction;
}

void EthernetMacHeader::setTree_direction(int tree_direction)
{
    handleChange();
    this->tree_direction = tree_direction;
}

unsigned long EthernetMacHeader::getQuery_id() const
{
    return this->query_id;
}

void EthernetMacHeader::setQuery_id(unsigned long query_id)
{
    handleChange();
    this->query_id = query_id;
}

unsigned long EthernetMacHeader::getFlow_id() const
{
    return this->flow_id;
}

void EthernetMacHeader::setFlow_id(unsigned long flow_id)
{
    handleChange();
    this->flow_id = flow_id;
}

unsigned int EthernetMacHeader::getCollective_scale() const
{
    return this->collective_scale;
}

void EthernetMacHeader::setCollective_scale(unsigned int collective_scale)
{
    handleChange();
    this->collective_scale = collective_scale;
}

int EthernetMacHeader::getCollective_alg() const
{
    return this->collective_alg;
}

void EthernetMacHeader::setCollective_alg(int collective_alg)
{
    handleChange();
    this->collective_alg = collective_alg;
}

int EthernetMacHeader::getPfc_signal() const
{
    return this->pfc_signal;
}

void EthernetMacHeader::setPfc_signal(int pfc_signal)
{
    handleChange();
    this->pfc_signal = pfc_signal;
}

bool EthernetMacHeader::getIs_ecn_marked() const
{
    return this->is_ecn_marked;
}

void EthernetMacHeader::setIs_ecn_marked(bool is_ecn_marked)
{
    handleChange();
    this->is_ecn_marked = is_ecn_marked;
}

unsigned long EthernetMacHeader::getCnp_flow_id() const
{
    return this->cnp_flow_id;
}

void EthernetMacHeader::setCnp_flow_id(unsigned long cnp_flow_id)
{
    handleChange();
    this->cnp_flow_id = cnp_flow_id;
}

int EthernetMacHeader::getCnp_src_gpu_id() const
{
    return this->cnp_src_gpu_id;
}

void EthernetMacHeader::setCnp_src_gpu_id(int cnp_src_gpu_id)
{
    handleChange();
    this->cnp_src_gpu_id = cnp_src_gpu_id;
}

int EthernetMacHeader::getSrc_gpu_idx() const
{
    return this->src_gpu_idx;
}

void EthernetMacHeader::setSrc_gpu_idx(int src_gpu_idx)
{
    handleChange();
    this->src_gpu_idx = src_gpu_idx;
}

unsigned long EthernetMacHeader::getSeq_num() const
{
    return this->seq_num;
}

void EthernetMacHeader::setSeq_num(unsigned long seq_num)
{
    handleChange();
    this->seq_num = seq_num;
}

bool EthernetMacHeader::getOrca_pkt_seen_agent() const
{
    return this->orca_pkt_seen_agent;
}

void EthernetMacHeader::setOrca_pkt_seen_agent(bool orca_pkt_seen_agent)
{
    handleChange();
    this->orca_pkt_seen_agent = orca_pkt_seen_agent;
}

int EthernetMacHeader::getOrca_agent_port_index() const
{
    return this->orca_agent_port_index;
}

void EthernetMacHeader::setOrca_agent_port_index(int orca_agent_port_index)
{
    handleChange();
    this->orca_agent_port_index = orca_agent_port_index;
}

size_t EthernetMacHeader::getRsbf_seed_listArraySize() const
{
    return rsbf_seed_list_arraysize;
}

unsigned int EthernetMacHeader::getRsbf_seed_list(size_t k) const
{
    if (k >= rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size rsbf_seed_list_arraysize indexed by %lu", (unsigned long)k);
    return this->rsbf_seed_list[k];
}

void EthernetMacHeader::setRsbf_seed_listArraySize(size_t newSize)
{
    handleChange();
    unsigned int *rsbf_seed_list2 = (newSize==0) ? nullptr : new unsigned int[newSize];
    size_t minSize = rsbf_seed_list_arraysize < newSize ? rsbf_seed_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i];
    for (size_t i = minSize; i < newSize; i++)
        rsbf_seed_list2[i] = 0;
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = rsbf_seed_list2;
    rsbf_seed_list_arraysize = newSize;
}

void EthernetMacHeader::setRsbf_seed_list(size_t k, unsigned int rsbf_seed_list)
{
    if (k >= rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->rsbf_seed_list[k] = rsbf_seed_list;
}

void EthernetMacHeader::insertRsbf_seed_list(size_t k, unsigned int rsbf_seed_list)
{
    handleChange();
    if (k > rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = rsbf_seed_list_arraysize + 1;
    unsigned int *rsbf_seed_list2 = new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i];
    rsbf_seed_list2[k] = rsbf_seed_list;
    for (i = k + 1; i < newSize; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i-1];
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = rsbf_seed_list2;
    rsbf_seed_list_arraysize = newSize;
}

void EthernetMacHeader::insertRsbf_seed_list(unsigned int rsbf_seed_list)
{
    insertRsbf_seed_list(rsbf_seed_list_arraysize, rsbf_seed_list);
}

void EthernetMacHeader::eraseRsbf_seed_list(size_t k)
{
    if (k >= rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = rsbf_seed_list_arraysize - 1;
    unsigned int *rsbf_seed_list2 = (newSize == 0) ? nullptr : new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i];
    for (i = k; i < newSize; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i+1];
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = rsbf_seed_list2;
    rsbf_seed_list_arraysize = newSize;
}

size_t EthernetMacHeader::getRsbf_bitstream_listArraySize() const
{
    return rsbf_bitstream_list_arraysize;
}

const char * EthernetMacHeader::getRsbf_bitstream_list(size_t k) const
{
    if (k >= rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size rsbf_bitstream_list_arraysize indexed by %lu", (unsigned long)k);
    return this->rsbf_bitstream_list[k].c_str();
}

void EthernetMacHeader::setRsbf_bitstream_listArraySize(size_t newSize)
{
    handleChange();
    omnetpp::opp_string *rsbf_bitstream_list2 = (newSize==0) ? nullptr : new omnetpp::opp_string[newSize];
    size_t minSize = rsbf_bitstream_list_arraysize < newSize ? rsbf_bitstream_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i];
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = rsbf_bitstream_list2;
    rsbf_bitstream_list_arraysize = newSize;
}

void EthernetMacHeader::setRsbf_bitstream_list(size_t k, const char * rsbf_bitstream_list)
{
    if (k >= rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->rsbf_bitstream_list[k] = rsbf_bitstream_list;
}

void EthernetMacHeader::insertRsbf_bitstream_list(size_t k, const char * rsbf_bitstream_list)
{
    handleChange();
    if (k > rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = rsbf_bitstream_list_arraysize + 1;
    omnetpp::opp_string *rsbf_bitstream_list2 = new omnetpp::opp_string[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i];
    rsbf_bitstream_list2[k] = rsbf_bitstream_list;
    for (i = k + 1; i < newSize; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i-1];
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = rsbf_bitstream_list2;
    rsbf_bitstream_list_arraysize = newSize;
}

void EthernetMacHeader::insertRsbf_bitstream_list(const char * rsbf_bitstream_list)
{
    insertRsbf_bitstream_list(rsbf_bitstream_list_arraysize, rsbf_bitstream_list);
}

void EthernetMacHeader::eraseRsbf_bitstream_list(size_t k)
{
    if (k >= rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = rsbf_bitstream_list_arraysize - 1;
    omnetpp::opp_string *rsbf_bitstream_list2 = (newSize == 0) ? nullptr : new omnetpp::opp_string[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i];
    for (i = k; i < newSize; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i+1];
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = rsbf_bitstream_list2;
    rsbf_bitstream_list_arraysize = newSize;
}

bool EthernetMacHeader::getRsbf_redundant_send() const
{
    return this->rsbf_redundant_send;
}

void EthernetMacHeader::setRsbf_redundant_send(bool rsbf_redundant_send)
{
    handleChange();
    this->rsbf_redundant_send = rsbf_redundant_send;
}

int EthernetMacHeader::getDst_tor_idx() const
{
    return this->dst_tor_idx;
}

void EthernetMacHeader::setDst_tor_idx(int dst_tor_idx)
{
    handleChange();
    this->dst_tor_idx = dst_tor_idx;
}

size_t EthernetMacHeader::getPartial_mcast_core_pktArraySize() const
{
    return partial_mcast_core_pkt_arraysize;
}

const Packet * EthernetMacHeader::getPartial_mcast_core_pkt(size_t k) const
{
    if (k >= partial_mcast_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size partial_mcast_core_pkt_arraysize indexed by %lu", (unsigned long)k);
    return this->partial_mcast_core_pkt[k];
}

void EthernetMacHeader::setPartial_mcast_core_pktArraySize(size_t newSize)
{
    handleChange();
    Packet * *partial_mcast_core_pkt2 = (newSize==0) ? nullptr : new Packet *[newSize];
    size_t minSize = partial_mcast_core_pkt_arraysize < newSize ? partial_mcast_core_pkt_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        partial_mcast_core_pkt2[i] = this->partial_mcast_core_pkt[i];
    for (size_t i = minSize; i < newSize; i++)
        partial_mcast_core_pkt2[i] = nullptr;
    delete [] this->partial_mcast_core_pkt;
    this->partial_mcast_core_pkt = partial_mcast_core_pkt2;
    partial_mcast_core_pkt_arraysize = newSize;
}

void EthernetMacHeader::setPartial_mcast_core_pkt(size_t k, Packet * partial_mcast_core_pkt)
{
    if (k >= partial_mcast_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->partial_mcast_core_pkt[k] = partial_mcast_core_pkt;
}

void EthernetMacHeader::insertPartial_mcast_core_pkt(size_t k, Packet * partial_mcast_core_pkt)
{
    handleChange();
    if (k > partial_mcast_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = partial_mcast_core_pkt_arraysize + 1;
    Packet * *partial_mcast_core_pkt2 = new Packet *[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        partial_mcast_core_pkt2[i] = this->partial_mcast_core_pkt[i];
    partial_mcast_core_pkt2[k] = partial_mcast_core_pkt;
    for (i = k + 1; i < newSize; i++)
        partial_mcast_core_pkt2[i] = this->partial_mcast_core_pkt[i-1];
    delete [] this->partial_mcast_core_pkt;
    this->partial_mcast_core_pkt = partial_mcast_core_pkt2;
    partial_mcast_core_pkt_arraysize = newSize;
}

void EthernetMacHeader::insertPartial_mcast_core_pkt(Packet * partial_mcast_core_pkt)
{
    insertPartial_mcast_core_pkt(partial_mcast_core_pkt_arraysize, partial_mcast_core_pkt);
}

void EthernetMacHeader::erasePartial_mcast_core_pkt(size_t k)
{
    if (k >= partial_mcast_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = partial_mcast_core_pkt_arraysize - 1;
    Packet * *partial_mcast_core_pkt2 = (newSize == 0) ? nullptr : new Packet *[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        partial_mcast_core_pkt2[i] = this->partial_mcast_core_pkt[i];
    for (i = k; i < newSize; i++)
        partial_mcast_core_pkt2[i] = this->partial_mcast_core_pkt[i+1];
    delete [] this->partial_mcast_core_pkt;
    this->partial_mcast_core_pkt = partial_mcast_core_pkt2;
    partial_mcast_core_pkt_arraysize = newSize;
}

bool EthernetMacHeader::getMulticast_pkt_in_programmable_core() const
{
    return this->multicast_pkt_in_programmable_core;
}

void EthernetMacHeader::setMulticast_pkt_in_programmable_core(bool multicast_pkt_in_programmable_core)
{
    handleChange();
    this->multicast_pkt_in_programmable_core = multicast_pkt_in_programmable_core;
}

const char * EthernetMacHeader::getAgg_cidr_group_id() const
{
    return this->agg_cidr_group_id.c_str();
}

void EthernetMacHeader::setAgg_cidr_group_id(const char * agg_cidr_group_id)
{
    handleChange();
    this->agg_cidr_group_id = agg_cidr_group_id;
}

unsigned long EthernetMacHeader::getAgg_cidr_group_member_count() const
{
    return this->agg_cidr_group_member_count;
}

void EthernetMacHeader::setAgg_cidr_group_member_count(unsigned long agg_cidr_group_member_count)
{
    handleChange();
    this->agg_cidr_group_member_count = agg_cidr_group_member_count;
}

bool EthernetMacHeader::getIs_redundant_cidr_pkt() const
{
    return this->is_redundant_cidr_pkt;
}

void EthernetMacHeader::setIs_redundant_cidr_pkt(bool is_redundant_cidr_pkt)
{
    handleChange();
    this->is_redundant_cidr_pkt = is_redundant_cidr_pkt;
}

size_t EthernetMacHeader::getCIDR_agg_pktArraySize() const
{
    return CIDR_agg_pkt_arraysize;
}

const Packet * EthernetMacHeader::getCIDR_agg_pkt(size_t k) const
{
    if (k >= CIDR_agg_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size CIDR_agg_pkt_arraysize indexed by %lu", (unsigned long)k);
    return this->CIDR_agg_pkt[k];
}

void EthernetMacHeader::setCIDR_agg_pktArraySize(size_t newSize)
{
    handleChange();
    Packet * *CIDR_agg_pkt2 = (newSize==0) ? nullptr : new Packet *[newSize];
    size_t minSize = CIDR_agg_pkt_arraysize < newSize ? CIDR_agg_pkt_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        CIDR_agg_pkt2[i] = this->CIDR_agg_pkt[i];
    for (size_t i = minSize; i < newSize; i++)
        CIDR_agg_pkt2[i] = nullptr;
    delete [] this->CIDR_agg_pkt;
    this->CIDR_agg_pkt = CIDR_agg_pkt2;
    CIDR_agg_pkt_arraysize = newSize;
}

void EthernetMacHeader::setCIDR_agg_pkt(size_t k, Packet * CIDR_agg_pkt)
{
    if (k >= CIDR_agg_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->CIDR_agg_pkt[k] = CIDR_agg_pkt;
}

void EthernetMacHeader::insertCIDR_agg_pkt(size_t k, Packet * CIDR_agg_pkt)
{
    handleChange();
    if (k > CIDR_agg_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = CIDR_agg_pkt_arraysize + 1;
    Packet * *CIDR_agg_pkt2 = new Packet *[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        CIDR_agg_pkt2[i] = this->CIDR_agg_pkt[i];
    CIDR_agg_pkt2[k] = CIDR_agg_pkt;
    for (i = k + 1; i < newSize; i++)
        CIDR_agg_pkt2[i] = this->CIDR_agg_pkt[i-1];
    delete [] this->CIDR_agg_pkt;
    this->CIDR_agg_pkt = CIDR_agg_pkt2;
    CIDR_agg_pkt_arraysize = newSize;
}

void EthernetMacHeader::insertCIDR_agg_pkt(Packet * CIDR_agg_pkt)
{
    insertCIDR_agg_pkt(CIDR_agg_pkt_arraysize, CIDR_agg_pkt);
}

void EthernetMacHeader::eraseCIDR_agg_pkt(size_t k)
{
    if (k >= CIDR_agg_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = CIDR_agg_pkt_arraysize - 1;
    Packet * *CIDR_agg_pkt2 = (newSize == 0) ? nullptr : new Packet *[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        CIDR_agg_pkt2[i] = this->CIDR_agg_pkt[i];
    for (i = k; i < newSize; i++)
        CIDR_agg_pkt2[i] = this->CIDR_agg_pkt[i+1];
    delete [] this->CIDR_agg_pkt;
    this->CIDR_agg_pkt = CIDR_agg_pkt2;
    CIDR_agg_pkt_arraysize = newSize;
}

bool EthernetMacHeader::getMulticast_pkt_in_CIDR_agg() const
{
    return this->multicast_pkt_in_CIDR_agg;
}

void EthernetMacHeader::setMulticast_pkt_in_CIDR_agg(bool multicast_pkt_in_CIDR_agg)
{
    handleChange();
    this->multicast_pkt_in_CIDR_agg = multicast_pkt_in_CIDR_agg;
}

const char * EthernetMacHeader::getCore_cidr_group_id() const
{
    return this->core_cidr_group_id.c_str();
}

void EthernetMacHeader::setCore_cidr_group_id(const char * core_cidr_group_id)
{
    handleChange();
    this->core_cidr_group_id = core_cidr_group_id;
}

unsigned long EthernetMacHeader::getCore_cidr_group_member_count() const
{
    return this->core_cidr_group_member_count;
}

void EthernetMacHeader::setCore_cidr_group_member_count(unsigned long core_cidr_group_member_count)
{
    handleChange();
    this->core_cidr_group_member_count = core_cidr_group_member_count;
}

size_t EthernetMacHeader::getCIDR_core_pktArraySize() const
{
    return CIDR_core_pkt_arraysize;
}

const Packet * EthernetMacHeader::getCIDR_core_pkt(size_t k) const
{
    if (k >= CIDR_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size CIDR_core_pkt_arraysize indexed by %lu", (unsigned long)k);
    return this->CIDR_core_pkt[k];
}

void EthernetMacHeader::setCIDR_core_pktArraySize(size_t newSize)
{
    handleChange();
    Packet * *CIDR_core_pkt2 = (newSize==0) ? nullptr : new Packet *[newSize];
    size_t minSize = CIDR_core_pkt_arraysize < newSize ? CIDR_core_pkt_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        CIDR_core_pkt2[i] = this->CIDR_core_pkt[i];
    for (size_t i = minSize; i < newSize; i++)
        CIDR_core_pkt2[i] = nullptr;
    delete [] this->CIDR_core_pkt;
    this->CIDR_core_pkt = CIDR_core_pkt2;
    CIDR_core_pkt_arraysize = newSize;
}

void EthernetMacHeader::setCIDR_core_pkt(size_t k, Packet * CIDR_core_pkt)
{
    if (k >= CIDR_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->CIDR_core_pkt[k] = CIDR_core_pkt;
}

void EthernetMacHeader::insertCIDR_core_pkt(size_t k, Packet * CIDR_core_pkt)
{
    handleChange();
    if (k > CIDR_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = CIDR_core_pkt_arraysize + 1;
    Packet * *CIDR_core_pkt2 = new Packet *[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        CIDR_core_pkt2[i] = this->CIDR_core_pkt[i];
    CIDR_core_pkt2[k] = CIDR_core_pkt;
    for (i = k + 1; i < newSize; i++)
        CIDR_core_pkt2[i] = this->CIDR_core_pkt[i-1];
    delete [] this->CIDR_core_pkt;
    this->CIDR_core_pkt = CIDR_core_pkt2;
    CIDR_core_pkt_arraysize = newSize;
}

void EthernetMacHeader::insertCIDR_core_pkt(Packet * CIDR_core_pkt)
{
    insertCIDR_core_pkt(CIDR_core_pkt_arraysize, CIDR_core_pkt);
}

void EthernetMacHeader::eraseCIDR_core_pkt(size_t k)
{
    if (k >= CIDR_core_pkt_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = CIDR_core_pkt_arraysize - 1;
    Packet * *CIDR_core_pkt2 = (newSize == 0) ? nullptr : new Packet *[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        CIDR_core_pkt2[i] = this->CIDR_core_pkt[i];
    for (i = k; i < newSize; i++)
        CIDR_core_pkt2[i] = this->CIDR_core_pkt[i+1];
    delete [] this->CIDR_core_pkt;
    this->CIDR_core_pkt = CIDR_core_pkt2;
    CIDR_core_pkt_arraysize = newSize;
}

bool EthernetMacHeader::getMulticast_pkt_in_CIDR_core() const
{
    return this->multicast_pkt_in_CIDR_core;
}

void EthernetMacHeader::setMulticast_pkt_in_CIDR_core(bool multicast_pkt_in_CIDR_core)
{
    handleChange();
    this->multicast_pkt_in_CIDR_core = multicast_pkt_in_CIDR_core;
}

const Packet * EthernetMacHeader::getOriginal_pkt() const
{
    return this->original_pkt;
}

void EthernetMacHeader::setOriginal_pkt(Packet * original_pkt)
{
    handleChange();
    this->original_pkt = original_pkt;
}

bool EthernetMacHeader::getIs_drop_notif() const
{
    return this->is_drop_notif;
}

void EthernetMacHeader::setIs_drop_notif(bool is_drop_notif)
{
    handleChange();
    this->is_drop_notif = is_drop_notif;
}

size_t EthernetMacHeader::getInput_ports_passedArraySize() const
{
    return input_ports_passed_arraysize;
}

unsigned int EthernetMacHeader::getInput_ports_passed(size_t k) const
{
    if (k >= input_ports_passed_arraysize) throw omnetpp::cRuntimeError("Array of size input_ports_passed_arraysize indexed by %lu", (unsigned long)k);
    return this->input_ports_passed[k];
}

void EthernetMacHeader::setInput_ports_passedArraySize(size_t newSize)
{
    handleChange();
    unsigned int *input_ports_passed2 = (newSize==0) ? nullptr : new unsigned int[newSize];
    size_t minSize = input_ports_passed_arraysize < newSize ? input_ports_passed_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        input_ports_passed2[i] = this->input_ports_passed[i];
    for (size_t i = minSize; i < newSize; i++)
        input_ports_passed2[i] = 0;
    delete [] this->input_ports_passed;
    this->input_ports_passed = input_ports_passed2;
    input_ports_passed_arraysize = newSize;
}

void EthernetMacHeader::setInput_ports_passed(size_t k, unsigned int input_ports_passed)
{
    if (k >= input_ports_passed_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->input_ports_passed[k] = input_ports_passed;
}

void EthernetMacHeader::insertInput_ports_passed(size_t k, unsigned int input_ports_passed)
{
    handleChange();
    if (k > input_ports_passed_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = input_ports_passed_arraysize + 1;
    unsigned int *input_ports_passed2 = new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        input_ports_passed2[i] = this->input_ports_passed[i];
    input_ports_passed2[k] = input_ports_passed;
    for (i = k + 1; i < newSize; i++)
        input_ports_passed2[i] = this->input_ports_passed[i-1];
    delete [] this->input_ports_passed;
    this->input_ports_passed = input_ports_passed2;
    input_ports_passed_arraysize = newSize;
}

void EthernetMacHeader::insertInput_ports_passed(unsigned int input_ports_passed)
{
    insertInput_ports_passed(input_ports_passed_arraysize, input_ports_passed);
}

void EthernetMacHeader::eraseInput_ports_passed(size_t k)
{
    if (k >= input_ports_passed_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = input_ports_passed_arraysize - 1;
    unsigned int *input_ports_passed2 = (newSize == 0) ? nullptr : new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        input_ports_passed2[i] = this->input_ports_passed[i];
    for (i = k; i < newSize; i++)
        input_ports_passed2[i] = this->input_ports_passed[i+1];
    delete [] this->input_ports_passed;
    this->input_ports_passed = input_ports_passed2;
    input_ports_passed_arraysize = newSize;
}

const char * EthernetMacHeader::getOriginal_pkt_name() const
{
    return this->original_pkt_name.c_str();
}

void EthernetMacHeader::setOriginal_pkt_name(const char * original_pkt_name)
{
    handleChange();
    this->original_pkt_name = original_pkt_name;
}

omnetpp::simtime_t EthernetMacHeader::getRecovery_time() const
{
    return this->recovery_time;
}

void EthernetMacHeader::setRecovery_time(omnetpp::simtime_t recovery_time)
{
    handleChange();
    this->recovery_time = recovery_time;
}

size_t EthernetMacHeader::getAffected_addressesArraySize() const
{
    return affected_addresses_arraysize;
}

const MacAddress& EthernetMacHeader::getAffected_addresses(size_t k) const
{
    if (k >= affected_addresses_arraysize) throw omnetpp::cRuntimeError("Array of size affected_addresses_arraysize indexed by %lu", (unsigned long)k);
    return this->affected_addresses[k];
}

void EthernetMacHeader::setAffected_addressesArraySize(size_t newSize)
{
    handleChange();
    MacAddress *affected_addresses2 = (newSize==0) ? nullptr : new MacAddress[newSize];
    size_t minSize = affected_addresses_arraysize < newSize ? affected_addresses_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        affected_addresses2[i] = this->affected_addresses[i];
    delete [] this->affected_addresses;
    this->affected_addresses = affected_addresses2;
    affected_addresses_arraysize = newSize;
}

void EthernetMacHeader::setAffected_addresses(size_t k, const MacAddress& affected_addresses)
{
    if (k >= affected_addresses_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->affected_addresses[k] = affected_addresses;
}

void EthernetMacHeader::insertAffected_addresses(size_t k, const MacAddress& affected_addresses)
{
    handleChange();
    if (k > affected_addresses_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = affected_addresses_arraysize + 1;
    MacAddress *affected_addresses2 = new MacAddress[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        affected_addresses2[i] = this->affected_addresses[i];
    affected_addresses2[k] = affected_addresses;
    for (i = k + 1; i < newSize; i++)
        affected_addresses2[i] = this->affected_addresses[i-1];
    delete [] this->affected_addresses;
    this->affected_addresses = affected_addresses2;
    affected_addresses_arraysize = newSize;
}

void EthernetMacHeader::insertAffected_addresses(const MacAddress& affected_addresses)
{
    insertAffected_addresses(affected_addresses_arraysize, affected_addresses);
}

void EthernetMacHeader::eraseAffected_addresses(size_t k)
{
    if (k >= affected_addresses_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = affected_addresses_arraysize - 1;
    MacAddress *affected_addresses2 = (newSize == 0) ? nullptr : new MacAddress[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        affected_addresses2[i] = this->affected_addresses[i];
    for (i = k; i < newSize; i++)
        affected_addresses2[i] = this->affected_addresses[i+1];
    delete [] this->affected_addresses;
    this->affected_addresses = affected_addresses2;
    affected_addresses_arraysize = newSize;
}

omnetpp::simtime_t EthernetMacHeader::getSwift_send_time() const
{
    return this->swift_send_time;
}

void EthernetMacHeader::setSwift_send_time(omnetpp::simtime_t swift_send_time)
{
    handleChange();
    this->swift_send_time = swift_send_time;
}

long EthernetMacHeader::getCidr_base_flow_id() const
{
    return this->cidr_base_flow_id;
}

void EthernetMacHeader::setCidr_base_flow_id(long cidr_base_flow_id)
{
    handleChange();
    this->cidr_base_flow_id = cidr_base_flow_id;
}

int EthernetMacHeader::getCidr_base_src_gpu_idx() const
{
    return this->cidr_base_src_gpu_idx;
}

void EthernetMacHeader::setCidr_base_src_gpu_idx(int cidr_base_src_gpu_idx)
{
    handleChange();
    this->cidr_base_src_gpu_idx = cidr_base_src_gpu_idx;
}

bool EthernetMacHeader::getIn_src_sharding() const
{
    return this->in_src_sharding;
}

void EthernetMacHeader::setIn_src_sharding(bool in_src_sharding)
{
    handleChange();
    this->in_src_sharding = in_src_sharding;
}

bool EthernetMacHeader::getUsing_orca() const
{
    return this->using_orca;
}

void EthernetMacHeader::setUsing_orca(bool using_orca)
{
    handleChange();
    this->using_orca = using_orca;
}

bool EthernetMacHeader::getUsing_elmo() const
{
    return this->using_elmo;
}

void EthernetMacHeader::setUsing_elmo(bool using_elmo)
{
    handleChange();
    this->using_elmo = using_elmo;
}

class EthernetMacHeaderDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
        FIELD_dest,
        FIELD_src,
        FIELD_cTag,
        FIELD_sTag,
        FIELD_typeOrLength,
        FIELD_hop_count,
        FIELD_prio_queue_idx,
        FIELD_isFB,
        FIELD_allow_same_input_output,
        FIELD_original_interface_id,
        FIELD_bouncedDistance,
        FIELD_maxBouncedDistance,
        FIELD_bouncedHop,
        FIELD_totalHopNum,
        FIELD_is_bursty,
        FIELD_payload_length,
        FIELD_total_length,
        FIELD_offset,
        FIELD_is_v2_dropped_packet_header,
        FIELD_queue_occupancy,
        FIELD_time_packet_received_at_nic,
        FIELD_local_nic_rx_delay,
        FIELD_remote_queueing_time,
        FIELD_fabric_delay_time_sent_from_source,
        FIELD_time_packet_sent_from_src,
        FIELD_is_deflected,
        FIELD_ports_to_dest_idx,
        FIELD_jump_to_idx,
        FIELD_dst_idx_list,
        FIELD_start_from_idx,
        FIELD_all_ports_to_dst,
        FIELD_all_jump_to_idx,
        FIELD_current_tree_idx,
        FIELD_ina_leaf_aggregation_num,
        FIELD_ina_spine_aggregation_num,
        FIELD_ina_core_aggregation_num,
        FIELD_tree_direction,
        FIELD_query_id,
        FIELD_flow_id,
        FIELD_collective_scale,
        FIELD_collective_alg,
        FIELD_pfc_signal,
        FIELD_is_ecn_marked,
        FIELD_cnp_flow_id,
        FIELD_cnp_src_gpu_id,
        FIELD_src_gpu_idx,
        FIELD_seq_num,
        FIELD_orca_pkt_seen_agent,
        FIELD_orca_agent_port_index,
        FIELD_rsbf_seed_list,
        FIELD_rsbf_bitstream_list,
        FIELD_rsbf_redundant_send,
        FIELD_dst_tor_idx,
        FIELD_partial_mcast_core_pkt,
        FIELD_multicast_pkt_in_programmable_core,
        FIELD_agg_cidr_group_id,
        FIELD_agg_cidr_group_member_count,
        FIELD_is_redundant_cidr_pkt,
        FIELD_CIDR_agg_pkt,
        FIELD_multicast_pkt_in_CIDR_agg,
        FIELD_core_cidr_group_id,
        FIELD_core_cidr_group_member_count,
        FIELD_CIDR_core_pkt,
        FIELD_multicast_pkt_in_CIDR_core,
        FIELD_original_pkt,
        FIELD_is_drop_notif,
        FIELD_input_ports_passed,
        FIELD_original_pkt_name,
        FIELD_recovery_time,
        FIELD_affected_addresses,
        FIELD_swift_send_time,
        FIELD_cidr_base_flow_id,
        FIELD_cidr_base_src_gpu_idx,
        FIELD_in_src_sharding,
        FIELD_using_orca,
        FIELD_using_elmo,
    };
  public:
    EthernetMacHeaderDescriptor();
    virtual ~EthernetMacHeaderDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(EthernetMacHeaderDescriptor)

EthernetMacHeaderDescriptor::EthernetMacHeaderDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::EthernetMacHeader)), "inet::FieldsChunk")
{
    propertynames = nullptr;
}

EthernetMacHeaderDescriptor::~EthernetMacHeaderDescriptor()
{
    delete[] propertynames;
}

bool EthernetMacHeaderDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<EthernetMacHeader *>(obj)!=nullptr;
}

const char **EthernetMacHeaderDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *EthernetMacHeaderDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int EthernetMacHeaderDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 76+basedesc->getFieldCount() : 76;
}

unsigned int EthernetMacHeaderDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        0,    // FIELD_dest
        0,    // FIELD_src
        FD_ISCOMPOUND | FD_ISPOINTER | FD_ISCOBJECT,    // FIELD_cTag
        FD_ISCOMPOUND | FD_ISPOINTER | FD_ISCOBJECT,    // FIELD_sTag
        FD_ISEDITABLE,    // FIELD_typeOrLength
        FD_ISEDITABLE,    // FIELD_hop_count
        FD_ISEDITABLE,    // FIELD_prio_queue_idx
        FD_ISEDITABLE,    // FIELD_isFB
        FD_ISEDITABLE,    // FIELD_allow_same_input_output
        FD_ISEDITABLE,    // FIELD_original_interface_id
        FD_ISEDITABLE,    // FIELD_bouncedDistance
        FD_ISEDITABLE,    // FIELD_maxBouncedDistance
        FD_ISEDITABLE,    // FIELD_bouncedHop
        FD_ISEDITABLE,    // FIELD_totalHopNum
        FD_ISEDITABLE,    // FIELD_is_bursty
        FD_ISEDITABLE,    // FIELD_payload_length
        FD_ISEDITABLE,    // FIELD_total_length
        FD_ISEDITABLE,    // FIELD_offset
        FD_ISEDITABLE,    // FIELD_is_v2_dropped_packet_header
        FD_ISEDITABLE,    // FIELD_queue_occupancy
        0,    // FIELD_time_packet_received_at_nic
        0,    // FIELD_local_nic_rx_delay
        0,    // FIELD_remote_queueing_time
        0,    // FIELD_fabric_delay_time_sent_from_source
        0,    // FIELD_time_packet_sent_from_src
        FD_ISEDITABLE,    // FIELD_is_deflected
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_ports_to_dest_idx
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_jump_to_idx
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_dst_idx_list
        FD_ISEDITABLE,    // FIELD_start_from_idx
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_all_ports_to_dst
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_all_jump_to_idx
        FD_ISEDITABLE,    // FIELD_current_tree_idx
        FD_ISEDITABLE,    // FIELD_ina_leaf_aggregation_num
        FD_ISEDITABLE,    // FIELD_ina_spine_aggregation_num
        FD_ISEDITABLE,    // FIELD_ina_core_aggregation_num
        FD_ISEDITABLE,    // FIELD_tree_direction
        FD_ISEDITABLE,    // FIELD_query_id
        FD_ISEDITABLE,    // FIELD_flow_id
        FD_ISEDITABLE,    // FIELD_collective_scale
        FD_ISEDITABLE,    // FIELD_collective_alg
        FD_ISEDITABLE,    // FIELD_pfc_signal
        FD_ISEDITABLE,    // FIELD_is_ecn_marked
        FD_ISEDITABLE,    // FIELD_cnp_flow_id
        FD_ISEDITABLE,    // FIELD_cnp_src_gpu_id
        FD_ISEDITABLE,    // FIELD_src_gpu_idx
        FD_ISEDITABLE,    // FIELD_seq_num
        FD_ISEDITABLE,    // FIELD_orca_pkt_seen_agent
        FD_ISEDITABLE,    // FIELD_orca_agent_port_index
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_rsbf_seed_list
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_rsbf_bitstream_list
        FD_ISEDITABLE,    // FIELD_rsbf_redundant_send
        FD_ISEDITABLE,    // FIELD_dst_tor_idx
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISPOINTER | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_partial_mcast_core_pkt
        FD_ISEDITABLE,    // FIELD_multicast_pkt_in_programmable_core
        FD_ISEDITABLE,    // FIELD_agg_cidr_group_id
        FD_ISEDITABLE,    // FIELD_agg_cidr_group_member_count
        FD_ISEDITABLE,    // FIELD_is_redundant_cidr_pkt
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISPOINTER | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_CIDR_agg_pkt
        FD_ISEDITABLE,    // FIELD_multicast_pkt_in_CIDR_agg
        FD_ISEDITABLE,    // FIELD_core_cidr_group_id
        FD_ISEDITABLE,    // FIELD_core_cidr_group_member_count
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISPOINTER | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_CIDR_core_pkt
        FD_ISEDITABLE,    // FIELD_multicast_pkt_in_CIDR_core
        FD_ISCOMPOUND | FD_ISPOINTER | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_original_pkt
        FD_ISEDITABLE,    // FIELD_is_drop_notif
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_input_ports_passed
        FD_ISEDITABLE,    // FIELD_original_pkt_name
        0,    // FIELD_recovery_time
        FD_ISARRAY,    // FIELD_affected_addresses
        0,    // FIELD_swift_send_time
        FD_ISEDITABLE,    // FIELD_cidr_base_flow_id
        FD_ISEDITABLE,    // FIELD_cidr_base_src_gpu_idx
        FD_ISEDITABLE,    // FIELD_in_src_sharding
        FD_ISEDITABLE,    // FIELD_using_orca
        FD_ISEDITABLE,    // FIELD_using_elmo
    };
    return (field >= 0 && field < 76) ? fieldTypeFlags[field] : 0;
}

const char *EthernetMacHeaderDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "dest",
        "src",
        "cTag",
        "sTag",
        "typeOrLength",
        "hop_count",
        "prio_queue_idx",
        "isFB",
        "allow_same_input_output",
        "original_interface_id",
        "bouncedDistance",
        "maxBouncedDistance",
        "bouncedHop",
        "totalHopNum",
        "is_bursty",
        "payload_length",
        "total_length",
        "offset",
        "is_v2_dropped_packet_header",
        "queue_occupancy",
        "time_packet_received_at_nic",
        "local_nic_rx_delay",
        "remote_queueing_time",
        "fabric_delay_time_sent_from_source",
        "time_packet_sent_from_src",
        "is_deflected",
        "ports_to_dest_idx",
        "jump_to_idx",
        "dst_idx_list",
        "start_from_idx",
        "all_ports_to_dst",
        "all_jump_to_idx",
        "current_tree_idx",
        "ina_leaf_aggregation_num",
        "ina_spine_aggregation_num",
        "ina_core_aggregation_num",
        "tree_direction",
        "query_id",
        "flow_id",
        "collective_scale",
        "collective_alg",
        "pfc_signal",
        "is_ecn_marked",
        "cnp_flow_id",
        "cnp_src_gpu_id",
        "src_gpu_idx",
        "seq_num",
        "orca_pkt_seen_agent",
        "orca_agent_port_index",
        "rsbf_seed_list",
        "rsbf_bitstream_list",
        "rsbf_redundant_send",
        "dst_tor_idx",
        "partial_mcast_core_pkt",
        "multicast_pkt_in_programmable_core",
        "agg_cidr_group_id",
        "agg_cidr_group_member_count",
        "is_redundant_cidr_pkt",
        "CIDR_agg_pkt",
        "multicast_pkt_in_CIDR_agg",
        "core_cidr_group_id",
        "core_cidr_group_member_count",
        "CIDR_core_pkt",
        "multicast_pkt_in_CIDR_core",
        "original_pkt",
        "is_drop_notif",
        "input_ports_passed",
        "original_pkt_name",
        "recovery_time",
        "affected_addresses",
        "swift_send_time",
        "cidr_base_flow_id",
        "cidr_base_src_gpu_idx",
        "in_src_sharding",
        "using_orca",
        "using_elmo",
    };
    return (field >= 0 && field < 76) ? fieldNames[field] : nullptr;
}

int EthernetMacHeaderDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0] == 'd' && strcmp(fieldName, "dest") == 0) return base+0;
    if (fieldName[0] == 's' && strcmp(fieldName, "src") == 0) return base+1;
    if (fieldName[0] == 'c' && strcmp(fieldName, "cTag") == 0) return base+2;
    if (fieldName[0] == 's' && strcmp(fieldName, "sTag") == 0) return base+3;
    if (fieldName[0] == 't' && strcmp(fieldName, "typeOrLength") == 0) return base+4;
    if (fieldName[0] == 'h' && strcmp(fieldName, "hop_count") == 0) return base+5;
    if (fieldName[0] == 'p' && strcmp(fieldName, "prio_queue_idx") == 0) return base+6;
    if (fieldName[0] == 'i' && strcmp(fieldName, "isFB") == 0) return base+7;
    if (fieldName[0] == 'a' && strcmp(fieldName, "allow_same_input_output") == 0) return base+8;
    if (fieldName[0] == 'o' && strcmp(fieldName, "original_interface_id") == 0) return base+9;
    if (fieldName[0] == 'b' && strcmp(fieldName, "bouncedDistance") == 0) return base+10;
    if (fieldName[0] == 'm' && strcmp(fieldName, "maxBouncedDistance") == 0) return base+11;
    if (fieldName[0] == 'b' && strcmp(fieldName, "bouncedHop") == 0) return base+12;
    if (fieldName[0] == 't' && strcmp(fieldName, "totalHopNum") == 0) return base+13;
    if (fieldName[0] == 'i' && strcmp(fieldName, "is_bursty") == 0) return base+14;
    if (fieldName[0] == 'p' && strcmp(fieldName, "payload_length") == 0) return base+15;
    if (fieldName[0] == 't' && strcmp(fieldName, "total_length") == 0) return base+16;
    if (fieldName[0] == 'o' && strcmp(fieldName, "offset") == 0) return base+17;
    if (fieldName[0] == 'i' && strcmp(fieldName, "is_v2_dropped_packet_header") == 0) return base+18;
    if (fieldName[0] == 'q' && strcmp(fieldName, "queue_occupancy") == 0) return base+19;
    if (fieldName[0] == 't' && strcmp(fieldName, "time_packet_received_at_nic") == 0) return base+20;
    if (fieldName[0] == 'l' && strcmp(fieldName, "local_nic_rx_delay") == 0) return base+21;
    if (fieldName[0] == 'r' && strcmp(fieldName, "remote_queueing_time") == 0) return base+22;
    if (fieldName[0] == 'f' && strcmp(fieldName, "fabric_delay_time_sent_from_source") == 0) return base+23;
    if (fieldName[0] == 't' && strcmp(fieldName, "time_packet_sent_from_src") == 0) return base+24;
    if (fieldName[0] == 'i' && strcmp(fieldName, "is_deflected") == 0) return base+25;
    if (fieldName[0] == 'p' && strcmp(fieldName, "ports_to_dest_idx") == 0) return base+26;
    if (fieldName[0] == 'j' && strcmp(fieldName, "jump_to_idx") == 0) return base+27;
    if (fieldName[0] == 'd' && strcmp(fieldName, "dst_idx_list") == 0) return base+28;
    if (fieldName[0] == 's' && strcmp(fieldName, "start_from_idx") == 0) return base+29;
    if (fieldName[0] == 'a' && strcmp(fieldName, "all_ports_to_dst") == 0) return base+30;
    if (fieldName[0] == 'a' && strcmp(fieldName, "all_jump_to_idx") == 0) return base+31;
    if (fieldName[0] == 'c' && strcmp(fieldName, "current_tree_idx") == 0) return base+32;
    if (fieldName[0] == 'i' && strcmp(fieldName, "ina_leaf_aggregation_num") == 0) return base+33;
    if (fieldName[0] == 'i' && strcmp(fieldName, "ina_spine_aggregation_num") == 0) return base+34;
    if (fieldName[0] == 'i' && strcmp(fieldName, "ina_core_aggregation_num") == 0) return base+35;
    if (fieldName[0] == 't' && strcmp(fieldName, "tree_direction") == 0) return base+36;
    if (fieldName[0] == 'q' && strcmp(fieldName, "query_id") == 0) return base+37;
    if (fieldName[0] == 'f' && strcmp(fieldName, "flow_id") == 0) return base+38;
    if (fieldName[0] == 'c' && strcmp(fieldName, "collective_scale") == 0) return base+39;
    if (fieldName[0] == 'c' && strcmp(fieldName, "collective_alg") == 0) return base+40;
    if (fieldName[0] == 'p' && strcmp(fieldName, "pfc_signal") == 0) return base+41;
    if (fieldName[0] == 'i' && strcmp(fieldName, "is_ecn_marked") == 0) return base+42;
    if (fieldName[0] == 'c' && strcmp(fieldName, "cnp_flow_id") == 0) return base+43;
    if (fieldName[0] == 'c' && strcmp(fieldName, "cnp_src_gpu_id") == 0) return base+44;
    if (fieldName[0] == 's' && strcmp(fieldName, "src_gpu_idx") == 0) return base+45;
    if (fieldName[0] == 's' && strcmp(fieldName, "seq_num") == 0) return base+46;
    if (fieldName[0] == 'o' && strcmp(fieldName, "orca_pkt_seen_agent") == 0) return base+47;
    if (fieldName[0] == 'o' && strcmp(fieldName, "orca_agent_port_index") == 0) return base+48;
    if (fieldName[0] == 'r' && strcmp(fieldName, "rsbf_seed_list") == 0) return base+49;
    if (fieldName[0] == 'r' && strcmp(fieldName, "rsbf_bitstream_list") == 0) return base+50;
    if (fieldName[0] == 'r' && strcmp(fieldName, "rsbf_redundant_send") == 0) return base+51;
    if (fieldName[0] == 'd' && strcmp(fieldName, "dst_tor_idx") == 0) return base+52;
    if (fieldName[0] == 'p' && strcmp(fieldName, "partial_mcast_core_pkt") == 0) return base+53;
    if (fieldName[0] == 'm' && strcmp(fieldName, "multicast_pkt_in_programmable_core") == 0) return base+54;
    if (fieldName[0] == 'a' && strcmp(fieldName, "agg_cidr_group_id") == 0) return base+55;
    if (fieldName[0] == 'a' && strcmp(fieldName, "agg_cidr_group_member_count") == 0) return base+56;
    if (fieldName[0] == 'i' && strcmp(fieldName, "is_redundant_cidr_pkt") == 0) return base+57;
    if (fieldName[0] == 'C' && strcmp(fieldName, "CIDR_agg_pkt") == 0) return base+58;
    if (fieldName[0] == 'm' && strcmp(fieldName, "multicast_pkt_in_CIDR_agg") == 0) return base+59;
    if (fieldName[0] == 'c' && strcmp(fieldName, "core_cidr_group_id") == 0) return base+60;
    if (fieldName[0] == 'c' && strcmp(fieldName, "core_cidr_group_member_count") == 0) return base+61;
    if (fieldName[0] == 'C' && strcmp(fieldName, "CIDR_core_pkt") == 0) return base+62;
    if (fieldName[0] == 'm' && strcmp(fieldName, "multicast_pkt_in_CIDR_core") == 0) return base+63;
    if (fieldName[0] == 'o' && strcmp(fieldName, "original_pkt") == 0) return base+64;
    if (fieldName[0] == 'i' && strcmp(fieldName, "is_drop_notif") == 0) return base+65;
    if (fieldName[0] == 'i' && strcmp(fieldName, "input_ports_passed") == 0) return base+66;
    if (fieldName[0] == 'o' && strcmp(fieldName, "original_pkt_name") == 0) return base+67;
    if (fieldName[0] == 'r' && strcmp(fieldName, "recovery_time") == 0) return base+68;
    if (fieldName[0] == 'a' && strcmp(fieldName, "affected_addresses") == 0) return base+69;
    if (fieldName[0] == 's' && strcmp(fieldName, "swift_send_time") == 0) return base+70;
    if (fieldName[0] == 'c' && strcmp(fieldName, "cidr_base_flow_id") == 0) return base+71;
    if (fieldName[0] == 'c' && strcmp(fieldName, "cidr_base_src_gpu_idx") == 0) return base+72;
    if (fieldName[0] == 'i' && strcmp(fieldName, "in_src_sharding") == 0) return base+73;
    if (fieldName[0] == 'u' && strcmp(fieldName, "using_orca") == 0) return base+74;
    if (fieldName[0] == 'u' && strcmp(fieldName, "using_elmo") == 0) return base+75;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *EthernetMacHeaderDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "inet::MacAddress",    // FIELD_dest
        "inet::MacAddress",    // FIELD_src
        "inet::Ieee8021qHeader",    // FIELD_cTag
        "inet::Ieee8021qHeader",    // FIELD_sTag
        "int",    // FIELD_typeOrLength
        "int",    // FIELD_hop_count
        "int",    // FIELD_prio_queue_idx
        "bool",    // FIELD_isFB
        "bool",    // FIELD_allow_same_input_output
        "int",    // FIELD_original_interface_id
        "int",    // FIELD_bouncedDistance
        "int",    // FIELD_maxBouncedDistance
        "int",    // FIELD_bouncedHop
        "int",    // FIELD_totalHopNum
        "bool",    // FIELD_is_bursty
        "inet::b",    // FIELD_payload_length
        "inet::b",    // FIELD_total_length
        "inet::b",    // FIELD_offset
        "bool",    // FIELD_is_v2_dropped_packet_header
        "uint16_t",    // FIELD_queue_occupancy
        "omnetpp::simtime_t",    // FIELD_time_packet_received_at_nic
        "omnetpp::simtime_t",    // FIELD_local_nic_rx_delay
        "omnetpp::simtime_t",    // FIELD_remote_queueing_time
        "omnetpp::simtime_t",    // FIELD_fabric_delay_time_sent_from_source
        "omnetpp::simtime_t",    // FIELD_time_packet_sent_from_src
        "bool",    // FIELD_is_deflected
        "inet::InnerList",    // FIELD_ports_to_dest_idx
        "inet::InnerList",    // FIELD_jump_to_idx
        "int",    // FIELD_dst_idx_list
        "unsigned int",    // FIELD_start_from_idx
        "inet::TwoDInnerList",    // FIELD_all_ports_to_dst
        "inet::TwoDInnerList",    // FIELD_all_jump_to_idx
        "int",    // FIELD_current_tree_idx
        "int",    // FIELD_ina_leaf_aggregation_num
        "int",    // FIELD_ina_spine_aggregation_num
        "int",    // FIELD_ina_core_aggregation_num
        "int",    // FIELD_tree_direction
        "unsigned long",    // FIELD_query_id
        "unsigned long",    // FIELD_flow_id
        "unsigned int",    // FIELD_collective_scale
        "int",    // FIELD_collective_alg
        "int",    // FIELD_pfc_signal
        "bool",    // FIELD_is_ecn_marked
        "unsigned long",    // FIELD_cnp_flow_id
        "int",    // FIELD_cnp_src_gpu_id
        "int",    // FIELD_src_gpu_idx
        "unsigned long",    // FIELD_seq_num
        "bool",    // FIELD_orca_pkt_seen_agent
        "int",    // FIELD_orca_agent_port_index
        "unsigned int",    // FIELD_rsbf_seed_list
        "string",    // FIELD_rsbf_bitstream_list
        "bool",    // FIELD_rsbf_redundant_send
        "int",    // FIELD_dst_tor_idx
        "inet::Packet",    // FIELD_partial_mcast_core_pkt
        "bool",    // FIELD_multicast_pkt_in_programmable_core
        "string",    // FIELD_agg_cidr_group_id
        "unsigned long",    // FIELD_agg_cidr_group_member_count
        "bool",    // FIELD_is_redundant_cidr_pkt
        "inet::Packet",    // FIELD_CIDR_agg_pkt
        "bool",    // FIELD_multicast_pkt_in_CIDR_agg
        "string",    // FIELD_core_cidr_group_id
        "unsigned long",    // FIELD_core_cidr_group_member_count
        "inet::Packet",    // FIELD_CIDR_core_pkt
        "bool",    // FIELD_multicast_pkt_in_CIDR_core
        "inet::Packet",    // FIELD_original_pkt
        "bool",    // FIELD_is_drop_notif
        "unsigned int",    // FIELD_input_ports_passed
        "string",    // FIELD_original_pkt_name
        "omnetpp::simtime_t",    // FIELD_recovery_time
        "inet::MacAddress",    // FIELD_affected_addresses
        "omnetpp::simtime_t",    // FIELD_swift_send_time
        "long",    // FIELD_cidr_base_flow_id
        "int",    // FIELD_cidr_base_src_gpu_idx
        "bool",    // FIELD_in_src_sharding
        "bool",    // FIELD_using_orca
        "bool",    // FIELD_using_elmo
    };
    return (field >= 0 && field < 76) ? fieldTypeStrings[field] : nullptr;
}

const char **EthernetMacHeaderDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        case FIELD_cTag: {
            static const char *names[] = { "owned",  nullptr };
            return names;
        }
        case FIELD_sTag: {
            static const char *names[] = { "owned",  nullptr };
            return names;
        }
        default: return nullptr;
    }
}

const char *EthernetMacHeaderDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        case FIELD_cTag:
            if (!strcmp(propertyname, "owned")) return "";
            return nullptr;
        case FIELD_sTag:
            if (!strcmp(propertyname, "owned")) return "";
            return nullptr;
        default: return nullptr;
    }
}

int EthernetMacHeaderDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    EthernetMacHeader *pp = (EthernetMacHeader *)object; (void)pp;
    switch (field) {
        case FIELD_ports_to_dest_idx: return pp->getPorts_to_dest_idxArraySize();
        case FIELD_jump_to_idx: return pp->getJump_to_idxArraySize();
        case FIELD_dst_idx_list: return pp->getDst_idx_listArraySize();
        case FIELD_all_ports_to_dst: return pp->getAll_ports_to_dstArraySize();
        case FIELD_all_jump_to_idx: return pp->getAll_jump_to_idxArraySize();
        case FIELD_rsbf_seed_list: return pp->getRsbf_seed_listArraySize();
        case FIELD_rsbf_bitstream_list: return pp->getRsbf_bitstream_listArraySize();
        case FIELD_partial_mcast_core_pkt: return pp->getPartial_mcast_core_pktArraySize();
        case FIELD_CIDR_agg_pkt: return pp->getCIDR_agg_pktArraySize();
        case FIELD_CIDR_core_pkt: return pp->getCIDR_core_pktArraySize();
        case FIELD_input_ports_passed: return pp->getInput_ports_passedArraySize();
        case FIELD_affected_addresses: return pp->getAffected_addressesArraySize();
        default: return 0;
    }
}

const char *EthernetMacHeaderDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetMacHeader *pp = (EthernetMacHeader *)object; (void)pp;
    switch (field) {
        case FIELD_cTag: { const Ieee8021qHeader * value = pp->getCTag(); return omnetpp::opp_typename(typeid(*value)); }
        case FIELD_sTag: { const Ieee8021qHeader * value = pp->getSTag(); return omnetpp::opp_typename(typeid(*value)); }
        case FIELD_partial_mcast_core_pkt: { const Packet * value = pp->getPartial_mcast_core_pkt(i); return omnetpp::opp_typename(typeid(*value)); }
        case FIELD_CIDR_agg_pkt: { const Packet * value = pp->getCIDR_agg_pkt(i); return omnetpp::opp_typename(typeid(*value)); }
        case FIELD_CIDR_core_pkt: { const Packet * value = pp->getCIDR_core_pkt(i); return omnetpp::opp_typename(typeid(*value)); }
        case FIELD_original_pkt: { const Packet * value = pp->getOriginal_pkt(); return omnetpp::opp_typename(typeid(*value)); }
        default: return nullptr;
    }
}

std::string EthernetMacHeaderDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetMacHeader *pp = (EthernetMacHeader *)object; (void)pp;
    switch (field) {
        case FIELD_dest: return pp->getDest().str();
        case FIELD_src: return pp->getSrc().str();
        case FIELD_cTag: {std::stringstream out; out << pp->getCTag(); return out.str();}
        case FIELD_sTag: {std::stringstream out; out << pp->getSTag(); return out.str();}
        case FIELD_typeOrLength: return long2string(pp->getTypeOrLength());
        case FIELD_hop_count: return long2string(pp->getHop_count());
        case FIELD_prio_queue_idx: return long2string(pp->getPrio_queue_idx());
        case FIELD_isFB: return bool2string(pp->isFB());
        case FIELD_allow_same_input_output: return bool2string(pp->getAllow_same_input_output());
        case FIELD_original_interface_id: return long2string(pp->getOriginal_interface_id());
        case FIELD_bouncedDistance: return long2string(pp->getBouncedDistance());
        case FIELD_maxBouncedDistance: return long2string(pp->getMaxBouncedDistance());
        case FIELD_bouncedHop: return long2string(pp->getBouncedHop());
        case FIELD_totalHopNum: return long2string(pp->getTotalHopNum());
        case FIELD_is_bursty: return bool2string(pp->getIs_bursty());
        case FIELD_payload_length: return unit2string(pp->getPayload_length());
        case FIELD_total_length: return unit2string(pp->getTotal_length());
        case FIELD_offset: return unit2string(pp->getOffset());
        case FIELD_is_v2_dropped_packet_header: return bool2string(pp->getIs_v2_dropped_packet_header());
        case FIELD_queue_occupancy: return ulong2string(pp->getQueue_occupancy());
        case FIELD_time_packet_received_at_nic: return simtime2string(pp->getTime_packet_received_at_nic());
        case FIELD_local_nic_rx_delay: return simtime2string(pp->getLocal_nic_rx_delay());
        case FIELD_remote_queueing_time: return simtime2string(pp->getRemote_queueing_time());
        case FIELD_fabric_delay_time_sent_from_source: return simtime2string(pp->getFabric_delay_time_sent_from_source());
        case FIELD_time_packet_sent_from_src: return simtime2string(pp->getTime_packet_sent_from_src());
        case FIELD_is_deflected: return bool2string(pp->getIs_deflected());
        case FIELD_ports_to_dest_idx: {std::stringstream out; out << pp->getPorts_to_dest_idx(i); return out.str();}
        case FIELD_jump_to_idx: {std::stringstream out; out << pp->getJump_to_idx(i); return out.str();}
        case FIELD_dst_idx_list: return long2string(pp->getDst_idx_list(i));
        case FIELD_start_from_idx: return ulong2string(pp->getStart_from_idx());
        case FIELD_all_ports_to_dst: {std::stringstream out; out << pp->getAll_ports_to_dst(i); return out.str();}
        case FIELD_all_jump_to_idx: {std::stringstream out; out << pp->getAll_jump_to_idx(i); return out.str();}
        case FIELD_current_tree_idx: return long2string(pp->getCurrent_tree_idx());
        case FIELD_ina_leaf_aggregation_num: return long2string(pp->getIna_leaf_aggregation_num());
        case FIELD_ina_spine_aggregation_num: return long2string(pp->getIna_spine_aggregation_num());
        case FIELD_ina_core_aggregation_num: return long2string(pp->getIna_core_aggregation_num());
        case FIELD_tree_direction: return long2string(pp->getTree_direction());
        case FIELD_query_id: return ulong2string(pp->getQuery_id());
        case FIELD_flow_id: return ulong2string(pp->getFlow_id());
        case FIELD_collective_scale: return ulong2string(pp->getCollective_scale());
        case FIELD_collective_alg: return long2string(pp->getCollective_alg());
        case FIELD_pfc_signal: return long2string(pp->getPfc_signal());
        case FIELD_is_ecn_marked: return bool2string(pp->getIs_ecn_marked());
        case FIELD_cnp_flow_id: return ulong2string(pp->getCnp_flow_id());
        case FIELD_cnp_src_gpu_id: return long2string(pp->getCnp_src_gpu_id());
        case FIELD_src_gpu_idx: return long2string(pp->getSrc_gpu_idx());
        case FIELD_seq_num: return ulong2string(pp->getSeq_num());
        case FIELD_orca_pkt_seen_agent: return bool2string(pp->getOrca_pkt_seen_agent());
        case FIELD_orca_agent_port_index: return long2string(pp->getOrca_agent_port_index());
        case FIELD_rsbf_seed_list: return ulong2string(pp->getRsbf_seed_list(i));
        case FIELD_rsbf_bitstream_list: return oppstring2string(pp->getRsbf_bitstream_list(i));
        case FIELD_rsbf_redundant_send: return bool2string(pp->getRsbf_redundant_send());
        case FIELD_dst_tor_idx: return long2string(pp->getDst_tor_idx());
        case FIELD_partial_mcast_core_pkt: {std::stringstream out; out << pp->getPartial_mcast_core_pkt(i); return out.str();}
        case FIELD_multicast_pkt_in_programmable_core: return bool2string(pp->getMulticast_pkt_in_programmable_core());
        case FIELD_agg_cidr_group_id: return oppstring2string(pp->getAgg_cidr_group_id());
        case FIELD_agg_cidr_group_member_count: return ulong2string(pp->getAgg_cidr_group_member_count());
        case FIELD_is_redundant_cidr_pkt: return bool2string(pp->getIs_redundant_cidr_pkt());
        case FIELD_CIDR_agg_pkt: {std::stringstream out; out << pp->getCIDR_agg_pkt(i); return out.str();}
        case FIELD_multicast_pkt_in_CIDR_agg: return bool2string(pp->getMulticast_pkt_in_CIDR_agg());
        case FIELD_core_cidr_group_id: return oppstring2string(pp->getCore_cidr_group_id());
        case FIELD_core_cidr_group_member_count: return ulong2string(pp->getCore_cidr_group_member_count());
        case FIELD_CIDR_core_pkt: {std::stringstream out; out << pp->getCIDR_core_pkt(i); return out.str();}
        case FIELD_multicast_pkt_in_CIDR_core: return bool2string(pp->getMulticast_pkt_in_CIDR_core());
        case FIELD_original_pkt: {std::stringstream out; out << pp->getOriginal_pkt(); return out.str();}
        case FIELD_is_drop_notif: return bool2string(pp->getIs_drop_notif());
        case FIELD_input_ports_passed: return ulong2string(pp->getInput_ports_passed(i));
        case FIELD_original_pkt_name: return oppstring2string(pp->getOriginal_pkt_name());
        case FIELD_recovery_time: return simtime2string(pp->getRecovery_time());
        case FIELD_affected_addresses: return pp->getAffected_addresses(i).str();
        case FIELD_swift_send_time: return simtime2string(pp->getSwift_send_time());
        case FIELD_cidr_base_flow_id: return long2string(pp->getCidr_base_flow_id());
        case FIELD_cidr_base_src_gpu_idx: return long2string(pp->getCidr_base_src_gpu_idx());
        case FIELD_in_src_sharding: return bool2string(pp->getIn_src_sharding());
        case FIELD_using_orca: return bool2string(pp->getUsing_orca());
        case FIELD_using_elmo: return bool2string(pp->getUsing_elmo());
        default: return "";
    }
}

bool EthernetMacHeaderDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    EthernetMacHeader *pp = (EthernetMacHeader *)object; (void)pp;
    switch (field) {
        case FIELD_typeOrLength: pp->setTypeOrLength(string2long(value)); return true;
        case FIELD_hop_count: pp->setHop_count(string2long(value)); return true;
        case FIELD_prio_queue_idx: pp->setPrio_queue_idx(string2long(value)); return true;
        case FIELD_isFB: pp->setIsFB(string2bool(value)); return true;
        case FIELD_allow_same_input_output: pp->setAllow_same_input_output(string2bool(value)); return true;
        case FIELD_original_interface_id: pp->setOriginal_interface_id(string2long(value)); return true;
        case FIELD_bouncedDistance: pp->setBouncedDistance(string2long(value)); return true;
        case FIELD_maxBouncedDistance: pp->setMaxBouncedDistance(string2long(value)); return true;
        case FIELD_bouncedHop: pp->setBouncedHop(string2long(value)); return true;
        case FIELD_totalHopNum: pp->setTotalHopNum(string2long(value)); return true;
        case FIELD_is_bursty: pp->setIs_bursty(string2bool(value)); return true;
        case FIELD_payload_length: pp->setPayload_length(b(string2long(value))); return true;
        case FIELD_total_length: pp->setTotal_length(b(string2long(value))); return true;
        case FIELD_offset: pp->setOffset(b(string2long(value))); return true;
        case FIELD_is_v2_dropped_packet_header: pp->setIs_v2_dropped_packet_header(string2bool(value)); return true;
        case FIELD_queue_occupancy: pp->setQueue_occupancy(string2ulong(value)); return true;
        case FIELD_is_deflected: pp->setIs_deflected(string2bool(value)); return true;
        case FIELD_dst_idx_list: pp->setDst_idx_list(i,string2long(value)); return true;
        case FIELD_start_from_idx: pp->setStart_from_idx(string2ulong(value)); return true;
        case FIELD_current_tree_idx: pp->setCurrent_tree_idx(string2long(value)); return true;
        case FIELD_ina_leaf_aggregation_num: pp->setIna_leaf_aggregation_num(string2long(value)); return true;
        case FIELD_ina_spine_aggregation_num: pp->setIna_spine_aggregation_num(string2long(value)); return true;
        case FIELD_ina_core_aggregation_num: pp->setIna_core_aggregation_num(string2long(value)); return true;
        case FIELD_tree_direction: pp->setTree_direction(string2long(value)); return true;
        case FIELD_query_id: pp->setQuery_id(string2ulong(value)); return true;
        case FIELD_flow_id: pp->setFlow_id(string2ulong(value)); return true;
        case FIELD_collective_scale: pp->setCollective_scale(string2ulong(value)); return true;
        case FIELD_collective_alg: pp->setCollective_alg(string2long(value)); return true;
        case FIELD_pfc_signal: pp->setPfc_signal(string2long(value)); return true;
        case FIELD_is_ecn_marked: pp->setIs_ecn_marked(string2bool(value)); return true;
        case FIELD_cnp_flow_id: pp->setCnp_flow_id(string2ulong(value)); return true;
        case FIELD_cnp_src_gpu_id: pp->setCnp_src_gpu_id(string2long(value)); return true;
        case FIELD_src_gpu_idx: pp->setSrc_gpu_idx(string2long(value)); return true;
        case FIELD_seq_num: pp->setSeq_num(string2ulong(value)); return true;
        case FIELD_orca_pkt_seen_agent: pp->setOrca_pkt_seen_agent(string2bool(value)); return true;
        case FIELD_orca_agent_port_index: pp->setOrca_agent_port_index(string2long(value)); return true;
        case FIELD_rsbf_seed_list: pp->setRsbf_seed_list(i,string2ulong(value)); return true;
        case FIELD_rsbf_bitstream_list: pp->setRsbf_bitstream_list(i,(value)); return true;
        case FIELD_rsbf_redundant_send: pp->setRsbf_redundant_send(string2bool(value)); return true;
        case FIELD_dst_tor_idx: pp->setDst_tor_idx(string2long(value)); return true;
        case FIELD_multicast_pkt_in_programmable_core: pp->setMulticast_pkt_in_programmable_core(string2bool(value)); return true;
        case FIELD_agg_cidr_group_id: pp->setAgg_cidr_group_id((value)); return true;
        case FIELD_agg_cidr_group_member_count: pp->setAgg_cidr_group_member_count(string2ulong(value)); return true;
        case FIELD_is_redundant_cidr_pkt: pp->setIs_redundant_cidr_pkt(string2bool(value)); return true;
        case FIELD_multicast_pkt_in_CIDR_agg: pp->setMulticast_pkt_in_CIDR_agg(string2bool(value)); return true;
        case FIELD_core_cidr_group_id: pp->setCore_cidr_group_id((value)); return true;
        case FIELD_core_cidr_group_member_count: pp->setCore_cidr_group_member_count(string2ulong(value)); return true;
        case FIELD_multicast_pkt_in_CIDR_core: pp->setMulticast_pkt_in_CIDR_core(string2bool(value)); return true;
        case FIELD_is_drop_notif: pp->setIs_drop_notif(string2bool(value)); return true;
        case FIELD_input_ports_passed: pp->setInput_ports_passed(i,string2ulong(value)); return true;
        case FIELD_original_pkt_name: pp->setOriginal_pkt_name((value)); return true;
        case FIELD_cidr_base_flow_id: pp->setCidr_base_flow_id(string2long(value)); return true;
        case FIELD_cidr_base_src_gpu_idx: pp->setCidr_base_src_gpu_idx(string2long(value)); return true;
        case FIELD_in_src_sharding: pp->setIn_src_sharding(string2bool(value)); return true;
        case FIELD_using_orca: pp->setUsing_orca(string2bool(value)); return true;
        case FIELD_using_elmo: pp->setUsing_elmo(string2bool(value)); return true;
        default: return false;
    }
}

const char *EthernetMacHeaderDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        case FIELD_cTag: return omnetpp::opp_typename(typeid(Ieee8021qHeader));
        case FIELD_sTag: return omnetpp::opp_typename(typeid(Ieee8021qHeader));
        case FIELD_ports_to_dest_idx: return omnetpp::opp_typename(typeid(InnerList));
        case FIELD_jump_to_idx: return omnetpp::opp_typename(typeid(InnerList));
        case FIELD_all_ports_to_dst: return omnetpp::opp_typename(typeid(TwoDInnerList));
        case FIELD_all_jump_to_idx: return omnetpp::opp_typename(typeid(TwoDInnerList));
        case FIELD_partial_mcast_core_pkt: return omnetpp::opp_typename(typeid(Packet));
        case FIELD_CIDR_agg_pkt: return omnetpp::opp_typename(typeid(Packet));
        case FIELD_CIDR_core_pkt: return omnetpp::opp_typename(typeid(Packet));
        case FIELD_original_pkt: return omnetpp::opp_typename(typeid(Packet));
        default: return nullptr;
    };
}

void *EthernetMacHeaderDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    EthernetMacHeader *pp = (EthernetMacHeader *)object; (void)pp;
    switch (field) {
        case FIELD_dest: return toVoidPtr(&pp->getDest()); break;
        case FIELD_src: return toVoidPtr(&pp->getSrc()); break;
        case FIELD_cTag: return toVoidPtr(pp->getCTag()); break;
        case FIELD_sTag: return toVoidPtr(pp->getSTag()); break;
        case FIELD_ports_to_dest_idx: return toVoidPtr(&pp->getPorts_to_dest_idx(i)); break;
        case FIELD_jump_to_idx: return toVoidPtr(&pp->getJump_to_idx(i)); break;
        case FIELD_all_ports_to_dst: return toVoidPtr(&pp->getAll_ports_to_dst(i)); break;
        case FIELD_all_jump_to_idx: return toVoidPtr(&pp->getAll_jump_to_idx(i)); break;
        case FIELD_partial_mcast_core_pkt: return toVoidPtr(pp->getPartial_mcast_core_pkt(i)); break;
        case FIELD_CIDR_agg_pkt: return toVoidPtr(pp->getCIDR_agg_pkt(i)); break;
        case FIELD_CIDR_core_pkt: return toVoidPtr(pp->getCIDR_core_pkt(i)); break;
        case FIELD_original_pkt: return toVoidPtr(pp->getOriginal_pkt()); break;
        case FIELD_affected_addresses: return toVoidPtr(&pp->getAffected_addresses(i)); break;
        default: return nullptr;
    }
}

Register_Class(EthernetControlFrame)

EthernetControlFrame::EthernetControlFrame() : ::inet::FieldsChunk()
{
}

EthernetControlFrame::EthernetControlFrame(const EthernetControlFrame& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

EthernetControlFrame::~EthernetControlFrame()
{
}

EthernetControlFrame& EthernetControlFrame::operator=(const EthernetControlFrame& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void EthernetControlFrame::copy(const EthernetControlFrame& other)
{
    this->opCode = other.opCode;
}

void EthernetControlFrame::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->opCode);
}

void EthernetControlFrame::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->opCode);
}

int EthernetControlFrame::getOpCode() const
{
    return this->opCode;
}

void EthernetControlFrame::setOpCode(int opCode)
{
    handleChange();
    this->opCode = opCode;
}

class EthernetControlFrameDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
        FIELD_opCode,
    };
  public:
    EthernetControlFrameDescriptor();
    virtual ~EthernetControlFrameDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(EthernetControlFrameDescriptor)

EthernetControlFrameDescriptor::EthernetControlFrameDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::EthernetControlFrame)), "inet::FieldsChunk")
{
    propertynames = nullptr;
}

EthernetControlFrameDescriptor::~EthernetControlFrameDescriptor()
{
    delete[] propertynames;
}

bool EthernetControlFrameDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<EthernetControlFrame *>(obj)!=nullptr;
}

const char **EthernetControlFrameDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *EthernetControlFrameDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int EthernetControlFrameDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 1+basedesc->getFieldCount() : 1;
}

unsigned int EthernetControlFrameDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_opCode
    };
    return (field >= 0 && field < 1) ? fieldTypeFlags[field] : 0;
}

const char *EthernetControlFrameDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "opCode",
    };
    return (field >= 0 && field < 1) ? fieldNames[field] : nullptr;
}

int EthernetControlFrameDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0] == 'o' && strcmp(fieldName, "opCode") == 0) return base+0;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *EthernetControlFrameDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "int",    // FIELD_opCode
    };
    return (field >= 0 && field < 1) ? fieldTypeStrings[field] : nullptr;
}

const char **EthernetControlFrameDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *EthernetControlFrameDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int EthernetControlFrameDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    EthernetControlFrame *pp = (EthernetControlFrame *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

const char *EthernetControlFrameDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetControlFrame *pp = (EthernetControlFrame *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string EthernetControlFrameDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetControlFrame *pp = (EthernetControlFrame *)object; (void)pp;
    switch (field) {
        case FIELD_opCode: return long2string(pp->getOpCode());
        default: return "";
    }
}

bool EthernetControlFrameDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    EthernetControlFrame *pp = (EthernetControlFrame *)object; (void)pp;
    switch (field) {
        case FIELD_opCode: pp->setOpCode(string2long(value)); return true;
        default: return false;
    }
}

const char *EthernetControlFrameDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

void *EthernetControlFrameDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    EthernetControlFrame *pp = (EthernetControlFrame *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

Register_Class(EthernetPauseFrame)

EthernetPauseFrame::EthernetPauseFrame() : ::inet::EthernetControlFrame()
{
    this->setChunkLength(ETHER_PAUSE_COMMAND_BYTES);
    this->setOpCode(ETHERNET_CONTROL_PAUSE);

}

EthernetPauseFrame::EthernetPauseFrame(const EthernetPauseFrame& other) : ::inet::EthernetControlFrame(other)
{
    copy(other);
}

EthernetPauseFrame::~EthernetPauseFrame()
{
}

EthernetPauseFrame& EthernetPauseFrame::operator=(const EthernetPauseFrame& other)
{
    if (this == &other) return *this;
    ::inet::EthernetControlFrame::operator=(other);
    copy(other);
    return *this;
}

void EthernetPauseFrame::copy(const EthernetPauseFrame& other)
{
    this->pauseTime = other.pauseTime;
}

void EthernetPauseFrame::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::EthernetControlFrame::parsimPack(b);
    doParsimPacking(b,this->pauseTime);
}

void EthernetPauseFrame::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::EthernetControlFrame::parsimUnpack(b);
    doParsimUnpacking(b,this->pauseTime);
}

int EthernetPauseFrame::getPauseTime() const
{
    return this->pauseTime;
}

void EthernetPauseFrame::setPauseTime(int pauseTime)
{
    handleChange();
    this->pauseTime = pauseTime;
}

class EthernetPauseFrameDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
        FIELD_pauseTime,
    };
  public:
    EthernetPauseFrameDescriptor();
    virtual ~EthernetPauseFrameDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(EthernetPauseFrameDescriptor)

EthernetPauseFrameDescriptor::EthernetPauseFrameDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::EthernetPauseFrame)), "inet::EthernetControlFrame")
{
    propertynames = nullptr;
}

EthernetPauseFrameDescriptor::~EthernetPauseFrameDescriptor()
{
    delete[] propertynames;
}

bool EthernetPauseFrameDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<EthernetPauseFrame *>(obj)!=nullptr;
}

const char **EthernetPauseFrameDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *EthernetPauseFrameDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int EthernetPauseFrameDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 1+basedesc->getFieldCount() : 1;
}

unsigned int EthernetPauseFrameDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_pauseTime
    };
    return (field >= 0 && field < 1) ? fieldTypeFlags[field] : 0;
}

const char *EthernetPauseFrameDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "pauseTime",
    };
    return (field >= 0 && field < 1) ? fieldNames[field] : nullptr;
}

int EthernetPauseFrameDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0] == 'p' && strcmp(fieldName, "pauseTime") == 0) return base+0;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *EthernetPauseFrameDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "int",    // FIELD_pauseTime
    };
    return (field >= 0 && field < 1) ? fieldTypeStrings[field] : nullptr;
}

const char **EthernetPauseFrameDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *EthernetPauseFrameDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int EthernetPauseFrameDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    EthernetPauseFrame *pp = (EthernetPauseFrame *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

const char *EthernetPauseFrameDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetPauseFrame *pp = (EthernetPauseFrame *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string EthernetPauseFrameDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetPauseFrame *pp = (EthernetPauseFrame *)object; (void)pp;
    switch (field) {
        case FIELD_pauseTime: return long2string(pp->getPauseTime());
        default: return "";
    }
}

bool EthernetPauseFrameDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    EthernetPauseFrame *pp = (EthernetPauseFrame *)object; (void)pp;
    switch (field) {
        case FIELD_pauseTime: pp->setPauseTime(string2long(value)); return true;
        default: return false;
    }
}

const char *EthernetPauseFrameDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

void *EthernetPauseFrameDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    EthernetPauseFrame *pp = (EthernetPauseFrame *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

Register_Class(EthernetPadding)

EthernetPadding::EthernetPadding() : ::inet::FieldsChunk()
{
}

EthernetPadding::EthernetPadding(const EthernetPadding& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

EthernetPadding::~EthernetPadding()
{
}

EthernetPadding& EthernetPadding::operator=(const EthernetPadding& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void EthernetPadding::copy(const EthernetPadding& other)
{
}

void EthernetPadding::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
}

void EthernetPadding::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
}

class EthernetPaddingDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
    };
  public:
    EthernetPaddingDescriptor();
    virtual ~EthernetPaddingDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(EthernetPaddingDescriptor)

EthernetPaddingDescriptor::EthernetPaddingDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::EthernetPadding)), "inet::FieldsChunk")
{
    propertynames = nullptr;
}

EthernetPaddingDescriptor::~EthernetPaddingDescriptor()
{
    delete[] propertynames;
}

bool EthernetPaddingDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<EthernetPadding *>(obj)!=nullptr;
}

const char **EthernetPaddingDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *EthernetPaddingDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int EthernetPaddingDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 0+basedesc->getFieldCount() : 0;
}

unsigned int EthernetPaddingDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    return 0;
}

const char *EthernetPaddingDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    return nullptr;
}

int EthernetPaddingDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *EthernetPaddingDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    return nullptr;
}

const char **EthernetPaddingDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *EthernetPaddingDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int EthernetPaddingDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    EthernetPadding *pp = (EthernetPadding *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

const char *EthernetPaddingDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetPadding *pp = (EthernetPadding *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string EthernetPaddingDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetPadding *pp = (EthernetPadding *)object; (void)pp;
    switch (field) {
        default: return "";
    }
}

bool EthernetPaddingDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    EthernetPadding *pp = (EthernetPadding *)object; (void)pp;
    switch (field) {
        default: return false;
    }
}

const char *EthernetPaddingDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    return nullptr;
}

void *EthernetPaddingDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    EthernetPadding *pp = (EthernetPadding *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

Register_Class(EthernetFcs)

EthernetFcs::EthernetFcs() : ::inet::FieldsChunk()
{
    this->setChunkLength(ETHER_FCS_BYTES);

}

EthernetFcs::EthernetFcs(const EthernetFcs& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

EthernetFcs::~EthernetFcs()
{
}

EthernetFcs& EthernetFcs::operator=(const EthernetFcs& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void EthernetFcs::copy(const EthernetFcs& other)
{
    this->fcs = other.fcs;
    this->fcsMode = other.fcsMode;
}

void EthernetFcs::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->fcs);
    doParsimPacking(b,this->fcsMode);
}

void EthernetFcs::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->fcs);
    doParsimUnpacking(b,this->fcsMode);
}

uint32_t EthernetFcs::getFcs() const
{
    return this->fcs;
}

void EthernetFcs::setFcs(uint32_t fcs)
{
    handleChange();
    this->fcs = fcs;
}

inet::FcsMode EthernetFcs::getFcsMode() const
{
    return this->fcsMode;
}

void EthernetFcs::setFcsMode(inet::FcsMode fcsMode)
{
    handleChange();
    this->fcsMode = fcsMode;
}

class EthernetFcsDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
        FIELD_fcs,
        FIELD_fcsMode,
    };
  public:
    EthernetFcsDescriptor();
    virtual ~EthernetFcsDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(EthernetFcsDescriptor)

EthernetFcsDescriptor::EthernetFcsDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::EthernetFcs)), "inet::FieldsChunk")
{
    propertynames = nullptr;
}

EthernetFcsDescriptor::~EthernetFcsDescriptor()
{
    delete[] propertynames;
}

bool EthernetFcsDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<EthernetFcs *>(obj)!=nullptr;
}

const char **EthernetFcsDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *EthernetFcsDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int EthernetFcsDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 2+basedesc->getFieldCount() : 2;
}

unsigned int EthernetFcsDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_fcs
        0,    // FIELD_fcsMode
    };
    return (field >= 0 && field < 2) ? fieldTypeFlags[field] : 0;
}

const char *EthernetFcsDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "fcs",
        "fcsMode",
    };
    return (field >= 0 && field < 2) ? fieldNames[field] : nullptr;
}

int EthernetFcsDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0] == 'f' && strcmp(fieldName, "fcs") == 0) return base+0;
    if (fieldName[0] == 'f' && strcmp(fieldName, "fcsMode") == 0) return base+1;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *EthernetFcsDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "uint32_t",    // FIELD_fcs
        "inet::FcsMode",    // FIELD_fcsMode
    };
    return (field >= 0 && field < 2) ? fieldTypeStrings[field] : nullptr;
}

const char **EthernetFcsDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        case FIELD_fcsMode: {
            static const char *names[] = { "enum",  nullptr };
            return names;
        }
        default: return nullptr;
    }
}

const char *EthernetFcsDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        case FIELD_fcsMode:
            if (!strcmp(propertyname, "enum")) return "inet::FcsMode";
            return nullptr;
        default: return nullptr;
    }
}

int EthernetFcsDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    EthernetFcs *pp = (EthernetFcs *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

const char *EthernetFcsDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetFcs *pp = (EthernetFcs *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string EthernetFcsDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    EthernetFcs *pp = (EthernetFcs *)object; (void)pp;
    switch (field) {
        case FIELD_fcs: return ulong2string(pp->getFcs());
        case FIELD_fcsMode: return enum2string(pp->getFcsMode(), "inet::FcsMode");
        default: return "";
    }
}

bool EthernetFcsDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    EthernetFcs *pp = (EthernetFcs *)object; (void)pp;
    switch (field) {
        case FIELD_fcs: pp->setFcs(string2ulong(value)); return true;
        default: return false;
    }
}

const char *EthernetFcsDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

void *EthernetFcsDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    EthernetFcs *pp = (EthernetFcs *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

} // namespace inet

