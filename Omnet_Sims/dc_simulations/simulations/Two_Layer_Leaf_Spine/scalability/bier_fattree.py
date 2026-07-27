from math import ceil
import math
from collections import defaultdict


SUPPORTED_BSLS = (64, 128, 256, 512, 1024, 2048, 4096)

def next_bsl(n, supported=SUPPORTED_BSLS, allow_extended=True):
    """
    Return the smallest BSL >= n.

    If n exceeds the largest value in `supported` and allow_extended=True,
    return the next power of two as an analytical extension.
    """
    for bsl in supported:
        if n <= bsl:
            return bsl

    if allow_extended:
        return 1 << (n - 1).bit_length()

    raise ValueError(f"Need BSL >= {n}, but supported set is only {supported}")


def validate_inputs(k, h, X):
    """
    Validate a k-ary fat-tree with:
      - k pods
      - k/2 aggregates per pod
      - k/2 edges per pod
      - k^2/4 cores
      - h hosts per edge
    """
    if k <= 0 or k % 2 != 0:
        raise ValueError("k must be a positive even integer")
    if h <= 0:
        raise ValueError("h must be a positive integer")
    if X < 0:
        raise ValueError("X must be >= 0")

    num_edges = (k * k) // 2
    total_hosts = num_edges * h

    if X > total_hosts - 1:
        raise ValueError(
            f"X={X} is too large: topology has only {total_hosts} hosts total, "
            f"so h1 can reach at most {total_hosts - 1} other hosts"
        )

    return {
        "pods": k,
        "aggs_per_pod": k // 2,
        "edges_per_pod": k // 2,
        "num_edges": num_edges,
        "num_aggs": num_edges,
        "num_cores": (k * k) // 4,
        "total_hosts": total_hosts,
    }


def edge_of_host(host_id, h):
    """Map 1-indexed host_id -> 1-indexed edge-switch id."""
    return (host_id - 1) // h + 1


def pod_of_edge(edge_id, k):
    """Map 1-indexed edge id -> 0-indexed pod id."""
    edges_per_pod = k // 2
    return (edge_id - 1) // edges_per_pod


def destination_edges(k, h, X):
    """
    Destination hosts are h2, h3, ..., h(1+X).
    Return sorted unique edge-switch ids that contain those hosts.
    """
    topo = validate_inputs(k, h, X)

    if X == 0:
        return []

    dst_edges = sorted({edge_of_host(host_id, h) for host_id in range(2, X + 2)})
    if dst_edges and dst_edges[-1] > topo["num_edges"]:
        raise ValueError(
            f"Destination edge id exceeds num_edges={topo['num_edges']}. "
            f"Check k, h, and X."
        )
    return dst_edges


def edges_by_pod(edge_ids, k):
    out = defaultdict(list)
    for edge_id in edge_ids:
        out[pod_of_edge(edge_id, k)].append(edge_id)
    return dict(out)


def topology_summary(k, h):
    topo = validate_inputs(k, h, 0)
    return {
        "k": k,
        "pods": topo["pods"],
        "aggregates_per_pod": topo["aggs_per_pod"],
        "edges_per_pod": topo["edges_per_pod"],
        "cores": topo["num_cores"],
        "total_edge_switches": topo["num_edges"],
        "hosts_per_edge": h,
    }


def bier_overhead_fattree(k, h, X, include_local_edge_bit=True):
    """
    Plain BIER model for k-ary fat-tree:
      - one BFER bit per edge switch
      - domain bit count = total number of edge switches = k^2 / 2
      - on-wire overhead = 96 fixed bits + BSL-bit BitString
      - set_bits_in_packet = number of destination edge switches
    """
    topo = validate_inputs(k, h, X)
    dst_edges = destination_edges(k, h, X)

    set_bits = len(dst_edges)
    if not include_local_edge_bit and 1 in dst_edges:
        set_bits -= 1

    domain_bits_needed = topo["num_edges"]
    bsl = next_bsl(domain_bits_needed)

    wire_overhead_bits = 96 + bsl
    wire_overhead_bytes = ceil(wire_overhead_bits / 8)

    return {
        "scheme": "BIER",
        "topology": topology_summary(k, h),
        "X": X,
        # "destination_edges": dst_edges,
        "num_destination_edges": len(dst_edges),
        "domain_bits_needed": domain_bits_needed,   # one bit per edge/BFER
        "bsl": bsl,
        "set_bits_in_packet": set_bits,
        "wire_overhead_bits": wire_overhead_bits,
        "wire_overhead_bytes": wire_overhead_bytes,
    }


def bierte_tree_link_bits_fattree(k, h, X):
    """
    Count BIER-TE *link* bits used by a simple explicit tree.

    Tree model:
      - source is edge1 in pod 0
      - if there are any non-local destination edges, choose one source aggregate
      - source edge -> chosen source aggregate : 1 link bit
      - same-pod destinations (other edges in pod 0):
          source aggregate -> each destination edge : 1 link bit each
      - for each remote pod with >= 1 destination edge:
          source aggregate -> one chosen core : 1 link bit
          chosen core -> one chosen destination aggregate : 1 link bit
          destination aggregate -> each destination edge in that pod : 1 link bit each

    Local delivery on edge1 does not consume a switch-to-switch TE link bit.
    """
    validate_inputs(k, h, X)
    dst_edges = destination_edges(k, h, X)

    if not dst_edges:
        return 0

    source_edge = 1
    source_pod = 0

    by_pod = edges_by_pod(dst_edges, k)

    same_pod_other_edges = [
        edge for edge in by_pod.get(source_pod, [])
        if edge != source_edge
    ]

    remote_pod_counts = {
        pod: len(edges)
        for pod, edges in by_pod.items()
        if pod != source_pod
    }

    nonlocal_dest_count = len(same_pod_other_edges) + sum(remote_pod_counts.values())
    if nonlocal_dest_count == 0:
        # Only local receivers on edge1 -> no switch-to-switch TE links needed
        return 0

    link_bits = 0

    # One source edge -> source aggregate link, shared by all non-local branches
    link_bits += 1

    # Same-pod fanout from the chosen source aggregate
    link_bits += len(same_pod_other_edges)

    # Each remote pod:
    #   source_agg -> chosen_core
    #   chosen_core -> destination_agg
    #   destination_agg -> each destination edge in that pod
    for _, q in remote_pod_counts.items():
        link_bits += 2 + q

    return link_bits


def bierte_overhead_fattree(k, h, X, leaf_decap_mode="shared"):
    """
    BIER-TE model for k-ary fat-tree:
      - one BP per P2P switch-switch link
      - total P2P link bits in domain:
          edge-agg links = k^3 / 4
          agg-core links = k^3 / 4
          total          = k^3 / 2
      - leaf decap bits:
          * 'shared' -> one shared local_decap bit for all destination edges
          * 'unique' -> one unique local_decap bit per edge switch
      - on-wire overhead = 96 fixed bits + BSL-bit BitString
      - set_bits_in_packet = tree link bits + decap bits used by this packet
    """
    topo = validate_inputs(k, h, X)
    dst_edges = destination_edges(k, h, X)

    p2p_link_bits_in_domain = (k ** 3) // 2

    if leaf_decap_mode == "shared":
        decap_bits_in_domain = 1
        decap_bits_in_packet = 1 if dst_edges else 0
    elif leaf_decap_mode == "unique":
        decap_bits_in_domain = topo["num_edges"]
        decap_bits_in_packet = len(dst_edges)
    else:
        raise ValueError("leaf_decap_mode must be 'shared' or 'unique'")

    domain_bits_needed = p2p_link_bits_in_domain + decap_bits_in_domain
    bsl = next_bsl(domain_bits_needed)

    link_bits_in_packet = bierte_tree_link_bits_fattree(k, h, X)
    set_bits_in_packet = link_bits_in_packet + decap_bits_in_packet

    wire_overhead_bits = 96 + bsl
    wire_overhead_bytes = ceil(wire_overhead_bits / 8)

    return {
        "scheme": f"BIER-TE ({leaf_decap_mode} leaf decap)",
        "topology": topology_summary(k, h),
        "X": X,
        # "destination_edges": dst_edges,
        "num_destination_edges": len(dst_edges),
        "p2p_link_bits_in_domain": p2p_link_bits_in_domain,
        "decap_bits_in_domain": decap_bits_in_domain,
        "domain_bits_needed": domain_bits_needed,
        "bsl": bsl,
        "link_bits_in_packet": link_bits_in_packet,
        "decap_bits_in_packet": decap_bits_in_packet,
        "set_bits_in_packet": set_bits_in_packet,
        "wire_overhead_bits": wire_overhead_bits,
        "wire_overhead_bytes": wire_overhead_bytes,
    }


def peel_overhead_fattree(k, h, X):
    """
    PEEL overhead model for k-ary fat-tree.

    Assumptions:
      - 8 GPUs per host
      - edge switch ports   = h*8 + k/2
      - aggregate ports     = k
      - core ports          = k
      - use the maximum switch radix in the network
    """
    validate_inputs(k, h, X)
    dst_edges = destination_edges(k, h, X)

    if not dst_edges:
        return {
            "scheme": "PEEL",
            "topology": topology_summary(k, h),
            "X": X,
            "destination_edges": [],
            "num_destination_edges": 0,
            "wire_overhead_bits": 0,
            "wire_overhead_bytes": 0,
        }

    # Assuming 8 GPUs per host
    edge_switch_ports = h * 8 + (k // 2)
    aggregate_switch_ports = k
    core_switch_ports = k

    switch_port_num = max(edge_switch_ports, aggregate_switch_ports, core_switch_ports)

    prefix_value = math.ceil(math.log2(switch_port_num))
    prefix_length = int(math.log2(prefix_value)) + 1 if prefix_value > 0 else 0

    wire_overhead_bits = 5 * (prefix_length + prefix_value)
    wire_overhead_bytes = ceil(wire_overhead_bits / 8.0)

    return {
        "scheme": "PEEL",
        "topology": topology_summary(k, h),
        "X": X,
        # "destination_edges": dst_edges,
        "num_destination_edges": len(dst_edges),
        "wire_overhead_bits": wire_overhead_bits,
        "wire_overhead_bytes": wire_overhead_bytes,
    }


def multicast_overheads_fattree(
    k,
    h,
    X,
    include_local_edge_bit=True,
    leaf_decap_mode="shared",
):
    return {
        # "bier": bier_overhead_fattree(
        #     k=k,
        #     h=h,
        #     X=X,
        #     include_local_edge_bit=include_local_edge_bit,
        # ),
        "bierte": bierte_overhead_fattree(
            k=k,
            h=h,
            X=X,
            leaf_decap_mode=leaf_decap_mode,
        ),
        "peel": peel_overhead_fattree(
            k=k,
            h=h,
            X=X,
        ),
    }


if __name__ == "__main__":
    from pprint import pprint

    # Example: vary k for a k-ary fat-tree, with h=2 hosts per edge,
    # multicast from h1 to h2..h16 (X=15).
    # for k in [4, 8, 16, 32]:
    #     result = multicast_overheads_fattree(
    #         k=k,
    #         h=2,
    #         X=15,
    #         leaf_decap_mode="shared",
    #     )
    #     pprint(result)
    #     print("\n--------------------\n")

    # k = 64
    # scale = 64
    # while (scale <= 16384):
    #     result = multicast_overheads_fattree(
    #         k=k,
    #         h=2,
    #         X=int(scale/8.0),   # assuming 8 gpus
    #         leaf_decap_mode="shared",
    #     )
    #     pprint(result)
    #     print("\n--------------------\n")
    #     scale *= 2

    # print('\n\n\n')


    # k = 8
    # scale = 128
    # while (k <= 256):
    #     result = multicast_overheads_fattree(
    #         k=k,
    #         h=2,
    #         X=int(scale/8.0),   # assuming 8 gpus
    #         leaf_decap_mode="shared",
    #     )
    #     pprint(result)
    #     print("\n--------------------\n")
    #     k *= 2

    # print('\n\n\n')


    k = 8
    receiver_percentages = 0.2
    while (k <= 256):
        num_nodes_under_each_leaf = 16
        total_nodes = int(k * k/2.0 * num_nodes_under_each_leaf)
        scale = receiver_percentages * total_nodes
        result = multicast_overheads_fattree(
            k=k,
            h=2,
            X=int(scale/8.0),   # assuming 8 gpus
            leaf_decap_mode="shared",
        )
        pprint(result)
        print("\n--------------------\n")
        k *= 2

    print('\n\n\n')

