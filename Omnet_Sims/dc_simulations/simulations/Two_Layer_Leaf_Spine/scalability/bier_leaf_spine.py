from math import ceil
import math


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

def validate_inputs(s, l, h, X):
    if s <= 0:
        raise ValueError("s must be a positive integer")
    if l <= 0:
        raise ValueError("l must be a positive integer")
    if h <= 0:
        raise ValueError("h must be a positive integer")

    total_hosts = l * h
    if X < 0:
        raise ValueError("X must be >= 0")
    if X > total_hosts - 1:
        raise ValueError(
            f"X={X} is too large: topology has only {total_hosts} hosts total, "
            f"so h1 can reach at most {total_hosts - 1} other hosts"
        )

    return total_hosts

def leaf_of_host(host_id, h):
    """Map 1-indexed host_id -> 1-indexed leaf id."""
    return (host_id - 1) // h + 1

def destination_leafs(l, h, X):
    """
    Destination hosts are h2, h3, ..., h(1+X).
    Return sorted unique leaf ids that contain those hosts.
    """
    total_hosts = l * h
    if X < 0:
        raise ValueError("X must be >= 0")
    if X > total_hosts - 1:
        raise ValueError(
            f"X={X} is too large: topology has only {total_hosts} hosts total, "
            f"so h1 can reach at most {total_hosts - 1} other hosts"
        )

    if X == 0:
        return []

    dst_leafs = sorted({leaf_of_host(host_id, h) for host_id in range(2, X + 2)})
    if dst_leafs and dst_leafs[-1] > l:
        raise ValueError(
            f"Destination leaf id exceeds l={l}. "
            f"Check l, h, and X."
        )
    return dst_leafs

def bier_overhead_leaf_spine(s, l, h, X, include_local_leaf_bit=True):
    """
    Plain BIER model for leaf-spine:
      - one BFER bit per leaf switch
      - domain bit count = l
      - on-wire overhead = 96 fixed bits + BSL-bit BitString
      - set_bits_in_packet = number of destination leafs
    """
    validate_inputs(s, l, h, X)
    dst_leafs = destination_leafs(l, h, X)

    set_bits = len(dst_leafs)
    if not include_local_leaf_bit and 1 in dst_leafs:
        set_bits -= 1

    domain_bits_needed = l
    bsl = next_bsl(domain_bits_needed)

    wire_overhead_bits = 96 + bsl
    wire_overhead_bytes = ceil(wire_overhead_bits / 8)

    return {
        "scheme": "BIER",
        "topology": {
            "spines": s,
            "leafs": l,
            "hosts_per_leaf": h,
        },
        "X": X,
        "destination_leafs": dst_leafs,
        "num_destination_leafs": len(dst_leafs),
        "domain_bits_needed": domain_bits_needed,   # one bit per leaf/BFER
        "bsl": bsl,
        "set_bits_in_packet": set_bits,
        "wire_overhead_bits": wire_overhead_bits,
        "wire_overhead_bytes": wire_overhead_bytes,
    }

def bierte_tree_link_bits_leaf_spine(s, l, h, X):
    """
    Count BIER-TE *link* bits used by a simple explicit tree.

    Tree model:
      - source is leaf1
      - if there are any remote destination leafs, choose one source spine
      - source leaf -> chosen spine : 1 link bit
      - chosen spine -> each remote destination leaf : one link bit each

    Local delivery on leaf1 does not consume a switch-to-switch TE link bit.
    """
    validate_inputs(s, l, h, X)
    dst_leafs = destination_leafs(l, h, X)

    if not dst_leafs:
        return 0

    remote_dst_leafs = [leaf for leaf in dst_leafs if leaf != 1]

    if not remote_dst_leafs:
        return 0

    # 1 bit for source leaf1 -> chosen spine
    # 1 bit per chosen spine -> remote destination leaf
    return 1 + len(remote_dst_leafs)

def bierte_overhead_leaf_spine(s, l, h, X, leaf_decap_mode="shared"):
    """
    BIER-TE model for leaf-spine:
      - one BP per leaf-spine link
      - total P2P link bits in domain = s * l
      - leaf decap bits:
          * 'shared' -> one shared local_decap bit for all destination leafs
          * 'unique' -> one unique local_decap bit per leaf
      - on-wire overhead = 96 fixed bits + BSL-bit BitString
      - set_bits_in_packet = tree link bits + decap bits used by this packet
    """
    validate_inputs(s, l, h, X)
    dst_leafs = destination_leafs(l, h, X)

    p2p_link_bits_in_domain = s * l

    if leaf_decap_mode == "shared":
        decap_bits_in_domain = 1
        decap_bits_in_packet = 1 if dst_leafs else 0
    elif leaf_decap_mode == "unique":
        decap_bits_in_domain = l
        decap_bits_in_packet = len(dst_leafs)
    else:
        raise ValueError("leaf_decap_mode must be 'shared' or 'unique'")

    domain_bits_needed = p2p_link_bits_in_domain + decap_bits_in_domain
    bsl = next_bsl(domain_bits_needed)

    link_bits_in_packet = bierte_tree_link_bits_leaf_spine(s, l, h, X)
    set_bits_in_packet = link_bits_in_packet + decap_bits_in_packet

    wire_overhead_bits = 96 + bsl
    wire_overhead_bytes = ceil(wire_overhead_bits / 8)

    return {
        "scheme": f"BIER-TE ({leaf_decap_mode} leaf decap)",
        "topology": {
            "spines": s,
            "leafs": l,
            "hosts_per_leaf": h,
        },
        "X": X,
        "destination_leafs": dst_leafs,
        "num_destination_leafs": len(dst_leafs),
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

def peel_overhead_leaf_spine(s, l, h, X):
    validate_inputs(s, l, h, X)

    dst_leafs = destination_leafs(l, h, X)

    if not dst_leafs:
        return {
            "scheme": "PEEL",
            "topology": {
                "spines": s,
                "leafs": l,
                "hosts_per_leaf": h,
            },
            "X": X,
            "destination_leafs": [],
            "num_destination_leafs": 0,
            "wire_overhead_bits": 0,
            "wire_overhead_bytes": 0,
        }

    # Assuming 8 GPUs per host
    switch_port_num = max(l, h * 8 + s)

    prefix_value = math.ceil(math.log2(switch_port_num))
    prefix_length = math.ceil(math.log2(prefix_value)) if prefix_value > 0 else 0

    wire_overhead_bits = 3 * (prefix_length + prefix_value)
    wire_overhead_bytes = ceil(wire_overhead_bits / 8.0)

    return {
        "scheme": "PEEL",
        "topology": {
            "spines": s,
            "leafs": l,
            "hosts_per_leaf": h,
        },
        "X": X,
        "destination_leafs": dst_leafs,
        "num_destination_leafs": len(dst_leafs),
        "wire_overhead_bits": wire_overhead_bits,
        "wire_overhead_bytes": wire_overhead_bytes,
    }

def multicast_overheads_leaf_spine(
    s,
    l,
    h,
    X,
    include_local_leaf_bit=True,
    leaf_decap_mode="shared",
):
    return {
        "bier": bier_overhead_leaf_spine(
            s=s,
            l=l,
            h=h,
            X=X,
            include_local_leaf_bit=include_local_leaf_bit,
        ),
        "bierte": bierte_overhead_leaf_spine(
            s=s,
            l=l,
            h=h,
            X=X,
            leaf_decap_mode=leaf_decap_mode,
        ),
        "peel": peel_overhead_leaf_spine(
            s=s,
            l=l,
            h=h,
            X=X,
        ),
    }

if __name__ == "__main__":
    # Example:
    # 16 spines, varying number of leafs, 2 hosts per leaf, multicast from h1 to h2..h16
    from pprint import pprint

    for l in [16, 32, 64, 128, 256, 512]:
        result = multicast_overheads_leaf_spine(
            s=16,
            l=l,
            h=2,
            X=15,
            leaf_decap_mode="shared",
        )
        pprint(result)
        print("\n--------------------\n")