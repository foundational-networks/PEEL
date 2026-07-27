from VARIABLES import *

class Flow:
    def __init__(self, rep_num=None, ID=None, send_time=None, is_bursty=False, is_ml=False, record_time=None):
        self.rep_num = rep_num
        self.ID = ID
        self.send_time = send_time
        self.end_time = None
        self.is_bursty = is_bursty
        self.is_ml = is_ml
        self.length = -1
        self.query_id = 0
        self.record_time = record_time


class Query:
    def __init__(self, start_time=None, ID=None, rep_num=None):
        self.num_completed_flows = 0
        self.num_flows =  None
        self.start_time = start_time
        self.rep_num = rep_num
        self.ID = ID
        self.end_time = None
        self.per_flow_response_size = None
        self.collective = None
        self.algorithm = None
        self.controller_delay = []
        self.old_num_flows = None

    def is_completed(self):
        if self.num_flows is None:
            raise Exception("Query: {}, start_time: {}, num_flows: {}".format(self.ID, self.start_time, self.num_flows))
        if (self.num_completed_flows > self.num_flows):
            raise Exception(f"(collective: {self.collective}, alg: {self.algorithm}): How can number of completed flows ({self.num_completed_flows}) be grater than number of flows per query ({self.num_flows})")
        if (VERBOSE and self.num_completed_flows != self.num_flows):
            print(f"Incomplete querry (collective: {self.collective}, alg: {self.algorithm}): {self.num_completed_flows} flows out of {self.num_flows} completed!")
        return self.num_completed_flows == self.num_flows


class V2PIFOPacketDropped:
    def __init__(self, recording_time, total_payload_len):
        self.recording_time = recording_time
        self.total_payload_len = total_payload_len
        self.seq = None
        self.ret_count = None


class Data:
    def __init__(self, rep_num, length, record_time):
        self.rep_num = rep_num
        self.length = length
        self.record_time = record_time
        self.total_length = None