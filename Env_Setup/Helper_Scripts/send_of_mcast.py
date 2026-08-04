#!/usr/bin/env python3

# To examine on the receiver, use: tcpdump -eni ens18 -v -p 'ether dst 01:00:5e:00:00:07'

from scapy.all import Ether, IP, UDP, Raw, sendp, get_if_hwaddr
import argparse
import time

TEST_MACS = {
    "all_network":        "00:00:00:12:34:56",
    "all_remote":         "c0:00:00:12:34:56",
    "106_host0_port3":    "c0:c0:c0:12:34:56",
    "107_host0_port3":    "c0:e0:c0:12:34:56",
    "106_ports3_6":       "c0:c0:40:12:34:56",
    "107_ports9_10":      "c0:e0:b0:12:34:56",
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iface", required=True, help="sender interface")
    ap.add_argument("--profile", choices=TEST_MACS.keys(), help="named test profile")
    ap.add_argument("--dst-mac", help="override dst MAC directly")
    ap.add_argument("--src-ip", default="10.105.0.10")
    ap.add_argument("--dst-ip", default="198.18.0.1")
    ap.add_argument("--sport", type=int, default=4000)
    ap.add_argument("--dport", type=int, default=5001)
    ap.add_argument("--count", type=int, default=5)
    ap.add_argument("--interval", type=float, default=1.0)
    ap.add_argument("--payload", default="OF-MCAST-TEST")
    args = ap.parse_args()

    if not args.dst_mac and not args.profile:
        raise SystemExit("use --profile or --dst-mac")

    dst_mac = args.dst_mac if args.dst_mac else TEST_MACS[args.profile]
    src_mac = get_if_hwaddr(args.iface)

    print(f"iface    : {args.iface}")
    print(f"src_mac  : {src_mac}")
    print(f"dst_mac  : {dst_mac}")
    print(f"src_ip   : {args.src_ip}")
    print(f"dst_ip   : {args.dst_ip}")
    print(f"dscp     : 7")
    print(f"udp dport: {args.dport}")
    print()

    for i in range(args.count):
        payload = f"{args.payload} seq={i} profile={args.profile or 'manual'}"
        pkt = (
            Ether(src=src_mac, dst=dst_mac) /
            IP(src=args.src_ip, dst=args.dst_ip, tos=(7 << 2)) /
            UDP(sport=args.sport, dport=args.dport) /
            Raw(load=payload.encode())
        )
        sendp(pkt, iface=args.iface, verbose=False)
        print(f"sent {i+1}/{args.count}: dst_mac={dst_mac} payload='{payload}'")
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
