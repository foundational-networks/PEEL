# Hop control bytes encoded in destination MAC:
#   TTL=64 -> byte0
#   TTL=63 -> byte1
#   TTL=62 -> byte2
#
#   - clear all existing flows/groups/meters before installing
#   - DSCP=7 only for experiment multicast rules
#   - ordinary misses fall back to NORMAL
#   - VLAN 144 is kept on trunk/uplink paths
#   - host-bound packets pop VLAN and rewrite dst MAC to 01:00:5e:00:00:07
#   - management port is kept under NORMAL behavior

from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER, set_ev_cls
from ryu.ofproto import ofproto_v1_3


class HierarchicalMulticast(app_manager.RyuApp):
    OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]

    # ---------- DPIDs ----------
    DPID_CORE = int("0000b8599f5c4400", 16)
    DPID_105  = int("0000f46b8c134345", 16)
    DPID_106  = int("0000f46b8c132d65", 16)
    DPID_107  = int("0000f46b8c1335c5", 16)

    # ---------- Experiment params ----------
    SPECIAL_DSCP = 7
    SPECIAL_NW_TOS = SPECIAL_DSCP << 2   # ovs-ofctl displays DSCP=7 as nw_tos=28
    EXP_VLAN = 144
    REWRITE_MCAST_MAC = "01:00:5e:00:00:07"

    # TTL controls which destination-MAC byte is interpreted.
    TTL_TO_MAC_BYTE = {
        64: 0,
        63: 1,
        62: 2,
    }

    # If True, a logical rule whose outputs have no configured physical port is
    # installed as a drop rule. If False, such a rule is skipped and NORMAL may
    # handle it. True is closer to "matched rule but no active output".
    INSTALL_EMPTY_LOGICAL_RULES_AS_DROP = True

    # ---------- Leaf/OVS ports ----------
    # On all OVS leaf switches:
    #   OF port 1 is the uplink, logical port 16.
    #   OF port 2 is the management port. Keep it NORMAL for VLAN 144/PVE access.
    SW105_UPLINK = 1
    SW105_MGMT   = 2
    SW106_UPLINK = 1
    SW106_MGMT   = 2
    SW107_UPLINK = 1
    SW107_MGMT   = 2

    # Logical node/VM ports 0..15 -> real OpenFlow ports.
    # Use None for logical ports not connected in the current setup.
    SW105_HOST_PORTS = [
        3, 4, None, None, None, None, None, None,
        None, None, None, None, None, None, None, None,
    ]

    SW106_HOST_PORTS = [
        3, 4, 5, 6, 7, 8, 9, 10,
        None, None, None, None, None, None, None, None,
    ]

    SW107_HOST_PORTS = [
        3, 4, 5, 6, 7, 8, 9, 10,
        None, None, None, None, None, None, None, None,
    ]

    # ---------- Core/spine ports ----------
    # logical core port 0 -> switch 105
    # logical core port 1 -> switch 106
    # logical core port 2 -> switch 107
    CORE_TO_105 = 103
    CORE_TO_106 = 73
    CORE_TO_107 = 75
    CORE_LEAF_PORTS = [CORE_TO_105, CORE_TO_106, CORE_TO_107]

    # Keep the old alias name for readability with prior script.
    CORE_FROM_105 = CORE_TO_105

    # ---------- Priorities ----------
    PRIO_MGMT_NORMAL = 1000
    PRIO_HIGH = 500
    PRIO_FALLBACK = 0

    def __init__(self, *args, **kwargs):
        super(HierarchicalMulticast, self).__init__(*args, **kwargs)

        # Requested leaf table, full logical leaf space.
        # 8-bit code = <3-bit prefix length> <5-bit prefix value>
        self.leaf_prefix_table = [
            (0b00100000, list(range(0, 16))),

            (0b01000000, list(range(0, 8))),
            (0b01001000, list(range(8, 16))),

            (0b01100000, list(range(0, 4))),
            (0b01100100, list(range(4, 8))),
            (0b01101000, list(range(8, 12))),
            (0b01101100, list(range(12, 16))),

            (0b10000000, [0, 1]),
            (0b10000010, [2, 3]),
            (0b10000100, [4, 5]),
            (0b10000110, [6, 7]),
            (0b10001000, [8, 9]),
            (0b10001010, [10, 11]),
            (0b10001100, [12, 13]),
            (0b10001110, [14, 15]),

            (0b10100000, [0]),
            (0b10100001, [1]),
            (0b10100010, [2]),
            (0b10100011, [3]),
            (0b10100100, [4]),
            (0b10100101, [5]),
            (0b10100110, [6]),
            (0b10100111, [7]),
            (0b10101000, [8]),
            (0b10101001, [9]),
            (0b10101010, [10]),
            (0b10101011, [11]),
            (0b10101100, [12]),
            (0b10101101, [13]),
            (0b10101110, [14]),
            (0b10101111, [15]),
            (0b10110000, [16]),
        ]

        # Requested spine/core table.
        self.core_prefix_table = [
            (0b10000000, [0, 1]),  # leafs 0..1
            (0b10100000, [0]),     # leaf 0
            (0b10100001, [1]),     # leaf 1
            (0b10100010, [2]),     # leaf 2
        ]

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _mac_str(self, arr):
        return ":".join("%02x" % x for x in arr)

    def _match_one_mac_byte_exact(self, byte_index, code_8b):
        value = [0, 0, 0, 0, 0, 0]
        mask  = [0, 0, 0, 0, 0, 0]
        value[byte_index] = code_8b & 0xFF
        mask[byte_index]  = 0xFF
        return self._mac_str(value), self._mac_str(mask)

    def _add_flow(self, dp, priority, match, actions):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        inst = [parser.OFPInstructionActions(ofp.OFPIT_APPLY_ACTIONS, actions)]
        mod = parser.OFPFlowMod(
            datapath=dp,
            priority=priority,
            match=match,
            instructions=inst
        )
        dp.send_msg(mod)

    def _add_group_all(self, dp, group_id, bucket_action_lists):
        ofp = dp.ofproto
        parser = dp.ofproto_parser

        # best-effort delete old group
        dp.send_msg(parser.OFPGroupMod(
            datapath=dp,
            command=ofp.OFPGC_DELETE,
            type_=ofp.OFPGT_ALL,
            group_id=group_id,
            buckets=[]
        ))

        buckets = []
        for acts in bucket_action_lists:
            buckets.append(parser.OFPBucket(
                weight=0,
                watch_port=ofp.OFPP_ANY,
                watch_group=ofp.OFPG_ANY,
                actions=acts
            ))

        dp.send_msg(parser.OFPGroupMod(
            datapath=dp,
            command=ofp.OFPGC_ADD,
            type_=ofp.OFPGT_ALL,
            group_id=group_id,
            buckets=buckets
        ))

    def _add_special_rule_both_ip_families(self, dp, priority, ttl,
                                           mac_byte_index, code_8b,
                                           actions, in_port=None):
        """Install DSCP+TTL+dst-mac-byte rules for IPv4 and IPv6.

        TTL matching is done with Ryu's Nicira extension match fields.  When
        nw_ttl is used, eth_type_nxm must also be used as the prerequisite.
        We use nw_tos=DSCP<<2 so ovs-ofctl displays the same nw_tos=28 style
        as the original script's ip_dscp=7 rules.
        """
        parser = dp.ofproto_parser
        eth_dst_val, eth_dst_mask = self._match_one_mac_byte_exact(
            mac_byte_index, code_8b
        )

        for eth_type in (0x0800, 0x86DD):
            fields = {
                "eth_type_nxm": eth_type,
                "nw_tos": self.SPECIAL_NW_TOS,
                "nw_ttl": ttl,
                "eth_dst_nxm": (eth_dst_val, eth_dst_mask),
            }
            if in_port is not None:
                fields["in_port_nxm"] = in_port

            match = parser.OFPMatch(**fields)
            self._add_flow(dp, priority, match, actions)

    def _trunk_flow_preamble_actions(self, dp, ttl):
        """Actions applied before a trunk/core group output.

        We use set_nw_ttl(ttl-1) instead of dec_nw_ttl. In this experiment
        each trunk rule is already TTL-specific, so the next TTL value is known
        when the FlowMod is generated.
        """
        parser = dp.ofproto_parser
        next_ttl = max(0, int(ttl) - 1)
        return [parser.OFPActionSetNwTtl(next_ttl)]

    def _trunk_bucket_actions(self, dp, out_port):
        parser = dp.ofproto_parser
        return [
            parser.OFPActionSetField(vlan_vid=(0x1000 | self.EXP_VLAN)),
            parser.OFPActionOutput(out_port),
        ]

    def _host_actions(self, dp, out_port):
        parser = dp.ofproto_parser
        return [
            parser.OFPActionPopVlan(),
            parser.OFPActionSetField(eth_dst=self.REWRITE_MCAST_MAC),
            parser.OFPActionOutput(out_port),
        ]

    def _install_normal_fallback(self, dp):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        self._add_flow(
            dp,
            self.PRIO_FALLBACK,
            parser.OFPMatch(),
            [parser.OFPActionOutput(ofp.OFPP_NORMAL)]
        )

    def _install_mgmt_normal(self, dp, mgmt_port):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        self._add_flow(
            dp,
            self.PRIO_MGMT_NORMAL,
            parser.OFPMatch(in_port=mgmt_port),
            [parser.OFPActionOutput(ofp.OFPP_NORMAL)]
        )

    def _leaf_ports_for_dpid(self, dpid):
        if dpid == self.DPID_105:
            return self.SW105_HOST_PORTS
        if dpid == self.DPID_106:
            return self.SW106_HOST_PORTS
        if dpid == self.DPID_107:
            return self.SW107_HOST_PORTS
        raise ValueError("Unknown leaf switch dpid")

    def _leaf_uplink_for_dpid(self, dpid):
        if dpid == self.DPID_105:
            return self.SW105_UPLINK
        if dpid == self.DPID_106:
            return self.SW106_UPLINK
        if dpid == self.DPID_107:
            return self.SW107_UPLINK
        raise ValueError("Unknown leaf switch dpid")

    def _leaf_mgmt_for_dpid(self, dpid):
        if dpid == self.DPID_105:
            return self.SW105_MGMT
        if dpid == self.DPID_106:
            return self.SW106_MGMT
        if dpid == self.DPID_107:
            return self.SW107_MGMT
        raise ValueError("Unknown leaf switch dpid")

    def _leaf_bucket_actions_for_logical_port(self, dp, host_ports, uplink,
                                              logical_port):
        if logical_port == 16:
            return self._trunk_bucket_actions(dp, uplink), True

        if logical_port < 0 or logical_port >= 16:
            raise ValueError("Invalid leaf logical port: %s" % logical_port)

        if logical_port >= len(host_ports):
            return None, False

        out_port = host_ports[logical_port]
        if out_port is None:
            return None, False

        return self._host_actions(dp, out_port), False

    def _send_barrier(self, dp):
        parser = dp.ofproto_parser
        dp.send_msg(parser.OFPBarrierRequest(dp))

    def _clear_all_flows(self, dp):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        mod = parser.OFPFlowMod(
            datapath=dp,
            table_id=ofp.OFPTT_ALL,
            command=ofp.OFPFC_DELETE,
            out_port=ofp.OFPP_ANY,
            out_group=ofp.OFPG_ANY,
            match=parser.OFPMatch()
        )
        dp.send_msg(mod)

    def _clear_all_groups(self, dp):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        dp.send_msg(parser.OFPGroupMod(
            datapath=dp,
            command=ofp.OFPGC_DELETE,
            type_=ofp.OFPGT_ALL,
            group_id=ofp.OFPG_ALL,
            buckets=[]
        ))

    def _clear_all_meters(self, dp):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        if hasattr(parser, "OFPMeterMod") and hasattr(ofp, "OFPM_ALL"):
            dp.send_msg(parser.OFPMeterMod(
                datapath=dp,
                command=ofp.OFPMC_DELETE,
                flags=0,
                meter_id=ofp.OFPM_ALL,
                bands=[]
            ))

    def _clear_switch_state(self, dp):
        self.logger.info("Clearing all flows/groups/meters on dpid=%016x", dp.id)
        self._clear_all_flows(dp)
        self._clear_all_groups(dp)
        self._clear_all_meters(dp)
        self._send_barrier(dp)

    # ------------------------------------------------------------------
    # Install rules
    # ------------------------------------------------------------------

    def _install_leaf(self, dp, sw_name):
        parser = dp.ofproto_parser

        if dp.id == self.DPID_105:
            base_gid = 3000
        elif dp.id == self.DPID_106:
            base_gid = 4000
        elif dp.id == self.DPID_107:
            base_gid = 5000
        else:
            raise ValueError("Unknown leaf switch dpid")

        host_ports = self._leaf_ports_for_dpid(dp.id)
        uplink = self._leaf_uplink_for_dpid(dp.id)
        mgmt = self._leaf_mgmt_for_dpid(dp.id)

        self._install_mgmt_normal(dp, mgmt)

        for idx, (code_8b, logical_ports) in enumerate(self.leaf_prefix_table):
            gid = base_gid + idx
            bucket_action_lists = []
            contains_trunk_output = False

            for logical_port in logical_ports:
                acts, is_trunk = self._leaf_bucket_actions_for_logical_port(
                    dp, host_ports, uplink, logical_port
                )
                if acts is None:
                    continue
                bucket_action_lists.append(acts)
                contains_trunk_output = contains_trunk_output or is_trunk

            if not bucket_action_lists:
                if self.INSTALL_EMPTY_LOGICAL_RULES_AS_DROP:
                    for ttl, mac_byte_index in self.TTL_TO_MAC_BYTE.items():
                        self._add_special_rule_both_ip_families(
                            dp=dp,
                            priority=self.PRIO_HIGH,
                            ttl=ttl,
                            mac_byte_index=mac_byte_index,
                            code_8b=code_8b,
                            actions=[]
                        )
                    continue
                else:
                    continue

            # Always use groups, including one-bucket groups such as b0 -> uplink.
            self._add_group_all(dp, gid, bucket_action_lists)

            for ttl, mac_byte_index in self.TTL_TO_MAC_BYTE.items():
                flow_actions = []
                if contains_trunk_output:
                    flow_actions.extend(self._trunk_flow_preamble_actions(dp, ttl))
                flow_actions.append(parser.OFPActionGroup(gid))

                self._add_special_rule_both_ip_families(
                    dp=dp,
                    priority=self.PRIO_HIGH,
                    ttl=ttl,
                    mac_byte_index=mac_byte_index,
                    code_8b=code_8b,
                    actions=flow_actions
                )

        self._install_normal_fallback(dp)
        self.logger.info(
            "Installed DSCP=%s TTL/MAC multicast rules + management NORMAL + fallback NORMAL on leaf %s",
            self.SPECIAL_DSCP,
            sw_name
        )

    def _install_core(self, dp):
        parser = dp.ofproto_parser
        base_gid = 2000

        for idx, (code_8b, logical_ports) in enumerate(self.core_prefix_table):
            gid = base_gid + idx
            bucket_action_lists = []

            for logical_port in logical_ports:
                if logical_port >= len(self.CORE_LEAF_PORTS):
                    continue
                out_port = self.CORE_LEAF_PORTS[logical_port]
                if out_port is None:
                    continue
                bucket_action_lists.append(self._trunk_bucket_actions(dp, out_port))

            if not bucket_action_lists:
                if self.INSTALL_EMPTY_LOGICAL_RULES_AS_DROP:
                    flow_actions = []
                else:
                    continue
            else:
                self._add_group_all(dp, gid, bucket_action_lists)
                flow_actions = self._trunk_flow_preamble_actions(dp, 63) + [parser.OFPActionGroup(gid)]

            # The core/spine is the second switch in the current 3-hop encoding.
            self._add_special_rule_both_ip_families(
                dp=dp,
                priority=self.PRIO_HIGH,
                ttl=63,
                mac_byte_index=1,
                code_8b=code_8b,
                actions=flow_actions
            )

        self._install_normal_fallback(dp)
        self.logger.info(
            "Installed DSCP=%s TTL/MAC multicast rules + fallback NORMAL on core",
            self.SPECIAL_DSCP
        )

    # ------------------------------------------------------------------
    # Event
    # ------------------------------------------------------------------

    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def switch_features_handler(self, ev):
        dp = ev.msg.datapath
        dpid = dp.id

        self.logger.info("Switch connected: dpid=%016x", dpid)
        self._clear_switch_state(dp)

        if dpid == self.DPID_105:
            self._install_leaf(dp, "105")
        elif dpid == self.DPID_CORE:
            self._install_core(dp)
        elif dpid == self.DPID_106:
            self._install_leaf(dp, "106")
        elif dpid == self.DPID_107:
            self._install_leaf(dp, "107")
        else:
            self.logger.info("No custom multicast config for dpid=%016x", dpid)

        self._send_barrier(dp)
        self.logger.info("Finished clean install on dpid=%016x", dpid)

