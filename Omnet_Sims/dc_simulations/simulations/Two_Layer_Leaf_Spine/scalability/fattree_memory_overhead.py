#!/usr/bin/env python3
"""
Estimate the memory overhead of storing a k-ary fat-tree topology at a host.

The script provides two complementary views:
1) An analytic estimate for a compact C/C++-style CSR adjacency-list representation.
2) An empirical measurement of the Python in-memory representation built by this script.

Topology model:
- k pods
- each pod has k/2 aggregation switches and k/2 ToR switches
- total core switches = k^2 / 4
- each ToR connects to h servers

Graph model stored at each host:
- node types
- links / adjacency information

Usage examples:
    python3 fattree_memory_overhead.py --k 8 --h 8
    python3 fattree_memory_overhead.py --k 32 --h 8 --skip-python-build
    python3 fattree_memory_overhead.py --k 16 --h 8 --node-type-bytes 1 --node-id-bytes 4 --offset-bytes 8
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from typing import List, Tuple


CORE = 0
AGG = 1
TOR = 2
SERVER = 3
NODE_TYPE_NAMES = {
    CORE: "core",
    AGG: "agg",
    TOR: "tor",
    SERVER: "server",
}


@dataclass
class FatTreeGraph:
    node_type: List[int]
    adjacency: List[List[int]]
    edges: List[Tuple[int, int]]



def format_bytes(num_bytes: int) -> str:
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    value = float(num_bytes)
    unit_idx = 0
    while value >= 1024.0 and unit_idx < len(units) - 1:
        value /= 1024.0
        unit_idx += 1
    return f"{value:.2f} {units[unit_idx]}"



def deep_getsizeof(obj, seen=None) -> int:
    """Recursively estimate Python object memory usage."""
    if seen is None:
        seen = set()

    obj_id = id(obj)
    if obj_id in seen:
        return 0
    seen.add(obj_id)

    size = sys.getsizeof(obj)

    if isinstance(obj, dict):
        size += sum(deep_getsizeof(k, seen) + deep_getsizeof(v, seen) for k, v in obj.items())
    elif isinstance(obj, (list, tuple, set, frozenset)):
        size += sum(deep_getsizeof(item, seen) for item in obj)
    elif hasattr(obj, "__dict__"):
        size += deep_getsizeof(vars(obj), seen)
    elif hasattr(obj, "__slots__"):
        for slot in obj.__slots__:
            if hasattr(obj, slot):
                size += deep_getsizeof(getattr(obj, slot), seen)

    return size



def fattree_counts(k: int, h: int):
    if k <= 0 or k % 2 != 0:
        raise ValueError("k must be a positive even integer")
    if h <= 0:
        raise ValueError("h must be a positive integer")

    num_core = (k * k) // 4
    num_agg = (k * k) // 2
    num_tor = (k * k) // 2
    num_servers = num_tor * h

    total_nodes = num_core + num_agg + num_tor + num_servers

    server_tor_links = num_servers
    tor_agg_links = k * (k // 2) * (k // 2)
    agg_core_links = k * (k // 2) * (k // 2)
    total_links = server_tor_links + tor_agg_links + agg_core_links

    return {
        "num_core": num_core,
        "num_agg": num_agg,
        "num_tor": num_tor,
        "num_servers": num_servers,
        "total_nodes": total_nodes,
        "server_tor_links": server_tor_links,
        "tor_agg_links": tor_agg_links,
        "agg_core_links": agg_core_links,
        "total_links": total_links,
    }



def build_kary_fattree(k: int, h: int) -> FatTreeGraph:
    """
    Build a canonical k-ary fat-tree.

    Core switches are arranged as k/2 groups, each with k/2 switches.
    Aggregation switch a in each pod connects to every core in core-group a.
    """
    counts = fattree_counts(k, h)
    num_pods = k
    aggs_per_pod = k // 2
    tors_per_pod = k // 2
    total_nodes = counts["total_nodes"]

    node_type = [None] * total_nodes
    adjacency = [[] for _ in range(total_nodes)]
    edges: List[Tuple[int, int]] = []

    next_id = 0

    # Core switches: grouped into k/2 groups, each of size k/2.
    core_ids = []
    for group_idx in range(k // 2):
        group = []
        for _ in range(k // 2):
            nid = next_id
            next_id += 1
            node_type[nid] = CORE
            group.append(nid)
        core_ids.append(group)

    # Pod-local aggregation and ToR switches.
    agg_ids = [[None] * aggs_per_pod for _ in range(num_pods)]
    tor_ids = [[None] * tors_per_pod for _ in range(num_pods)]

    for pod in range(num_pods):
        for a in range(aggs_per_pod):
            nid = next_id
            next_id += 1
            node_type[nid] = AGG
            agg_ids[pod][a] = nid
        for t in range(tors_per_pod):
            nid = next_id
            next_id += 1
            node_type[nid] = TOR
            tor_ids[pod][t] = nid

    # Servers under each ToR.
    for pod in range(num_pods):
        for t in range(tors_per_pod):
            tor = tor_ids[pod][t]
            for _ in range(h):
                nid = next_id
                next_id += 1
                node_type[nid] = SERVER
                adjacency[tor].append(nid)
                adjacency[nid].append(tor)
                edges.append((tor, nid))

    # ToR <-> Aggregation links within each pod.
    for pod in range(num_pods):
        for t in range(tors_per_pod):
            tor = tor_ids[pod][t]
            for a in range(aggs_per_pod):
                agg = agg_ids[pod][a]
                adjacency[tor].append(agg)
                adjacency[agg].append(tor)
                edges.append((tor, agg))

    # Aggregation <-> Core links.
    for pod in range(num_pods):
        for a in range(aggs_per_pod):
            agg = agg_ids[pod][a]
            for core in core_ids[a]:
                adjacency[agg].append(core)
                adjacency[core].append(agg)
                edges.append((agg, core))

    assert next_id == total_nodes
    assert len(edges) == counts["total_links"]

    return FatTreeGraph(node_type=node_type, adjacency=adjacency, edges=edges)



def estimate_compact_csr_bytes(
    num_nodes: int,
    num_links: int,
    node_type_bytes: int = 1,
    node_id_bytes: int = 4,
    offset_bytes: int = 8,
    per_link_metadata_bytes: int = 0,
) -> int:
    """
    Approximate bytes for a compact adjacency-list/CSR representation.

    Stored arrays:
    - node_type[num_nodes]
    - offsets[num_nodes + 1]
    - neighbors[2 * num_links]   # undirected links stored at both endpoints

    Optional per-link metadata can be added to each adjacency entry.
    """
    return (
        num_nodes * node_type_bytes
        + (num_nodes + 1) * offset_bytes
        + (2 * num_links) * (node_id_bytes + per_link_metadata_bytes)
    )



def estimate_edge_list_bytes(
    num_nodes: int,
    num_links: int,
    node_type_bytes: int = 1,
    node_id_bytes: int = 4,
    per_link_metadata_bytes: int = 0,
) -> int:
    """
    Approximate bytes for storing nodes plus an edge list.

    Stored arrays:
    - node_type[num_nodes]
    - edges[num_links] where each edge stores (u, v)
    """
    return num_nodes * node_type_bytes + num_links * (2 * node_id_bytes + per_link_metadata_bytes)



def summarize(k: int, h: int, build_python_graph: bool, node_type_bytes: int, node_id_bytes: int,
              offset_bytes: int, per_link_metadata_bytes: int) -> None:
    counts = fattree_counts(k, h)
    num_nodes = counts["total_nodes"]
    num_links = counts["total_links"]

    csr_bytes = estimate_compact_csr_bytes(
        num_nodes=num_nodes,
        num_links=num_links,
        node_type_bytes=node_type_bytes,
        node_id_bytes=node_id_bytes,
        offset_bytes=offset_bytes,
        per_link_metadata_bytes=per_link_metadata_bytes,
    )

    edge_list_bytes = estimate_edge_list_bytes(
        num_nodes=num_nodes,
        num_links=num_links,
        node_type_bytes=node_type_bytes,
        node_id_bytes=node_id_bytes,
        per_link_metadata_bytes=per_link_metadata_bytes,
    )

    print("=" * 72)
    print(f"k-ary fat-tree memory estimate (k={k}, h={h})")
    print("=" * 72)
    print("Topology counts")
    print(f"  Core switches        : {counts['num_core']}")
    print(f"  Aggregation switches : {counts['num_agg']}")
    print(f"  ToR switches         : {counts['num_tor']}")
    print(f"  Servers              : {counts['num_servers']}")
    print(f"  Total nodes          : {num_nodes}")
    print(f"  Server-ToR links     : {counts['server_tor_links']}")
    print(f"  ToR-Agg links        : {counts['tor_agg_links']}")
    print(f"  Agg-Core links       : {counts['agg_core_links']}")
    print(f"  Total undirected links: {num_links}")
    print()

    print("Analytic memory estimates")
    print(f"  Compact CSR adjacency list : {csr_bytes} bytes ({format_bytes(csr_bytes)})")
    print(f"  Edge-list representation   : {edge_list_bytes} bytes ({format_bytes(edge_list_bytes)})")
    print()

    print("CSR assumptions")
    print(f"  node_type_bytes           = {node_type_bytes}")
    print(f"  node_id_bytes             = {node_id_bytes}")
    print(f"  offset_bytes              = {offset_bytes}")
    print(f"  per_link_metadata_bytes   = {per_link_metadata_bytes}")
    print()

    print("Closed-form counts")
    print("  total_nodes = k^2/4 + k^2/2 + k^2/2 + (k^2/2)h = k^2(5 + 2h)/4")
    print("  total_links = (k^2/2)h + k^3/4 + k^3/4 = k^2(h + k)/2")
    print()

    if build_python_graph:
        graph = build_kary_fattree(k, h)
        py_graph_bytes = deep_getsizeof(graph)
        py_node_type_bytes = deep_getsizeof(graph.node_type)
        py_adj_bytes = deep_getsizeof(graph.adjacency)
        py_edges_bytes = deep_getsizeof(graph.edges)

        print("Empirical Python memory usage of the built graph")
        print(f"  Whole graph object         : {py_graph_bytes} bytes ({format_bytes(py_graph_bytes)})")
        print(f"  node_type list             : {py_node_type_bytes} bytes ({format_bytes(py_node_type_bytes)})")
        print(f"  adjacency list             : {py_adj_bytes} bytes ({format_bytes(py_adj_bytes)})")
        print(f"  edge list                  : {py_edges_bytes} bytes ({format_bytes(py_edges_bytes)})")
        print()
        print("Note: Python measurements are much larger than a compact C/C++ implementation")
        print("because Python objects and lists have substantial runtime overhead.")



def main() -> None:

    # python3 fat_tree_memory_overhead.py --k 128 --h 8

    parser = argparse.ArgumentParser(description="Estimate fat-tree topology memory overhead")
    parser.add_argument("--k", type=int, required=True, help="fat-tree parameter k (must be even)")
    parser.add_argument("--h", type=int, required=True, help="number of servers connected to each ToR")
    parser.add_argument("--node-type-bytes", type=int, default=1,
                        help="bytes used to store each node type in the compact estimate")
    parser.add_argument("--node-id-bytes", type=int, default=4,
                        help="bytes used to store each node id in the compact estimate")
    parser.add_argument("--offset-bytes", type=int, default=8,
                        help="bytes used to store each CSR offset entry")
    parser.add_argument("--per-link-metadata-bytes", type=int, default=0,
                        help="extra metadata bytes stored per adjacency entry or per edge")
    parser.add_argument("--skip-python-build", action="store_true",
                        help="skip building the Python graph and only print analytic estimates")
    args = parser.parse_args()

    summarize(
        k=args.k,
        h=args.h,
        build_python_graph=not args.skip_python_build,
        node_type_bytes=args.node_type_bytes,
        node_id_bytes=args.node_id_bytes,
        offset_bytes=args.offset_bytes,
        per_link_metadata_bytes=args.per_link_metadata_bytes,
    )


if __name__ == "__main__":
    main()
