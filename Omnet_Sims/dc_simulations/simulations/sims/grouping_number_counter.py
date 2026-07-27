import math

def combination(a, b):
    assert(b <= a)
    if (a-b < b):
        result = 1.0 / math.factorial(a-b)
        for i in range(b+1, a+1):
            result *= i
    else:
        result = 1.0 / math.factorial(b)
        for i in range(a-b+1, a+1):
            result *= i
    return result

NUM_TORS = 24
# 2, 4, 16
NUM_SERVERS_PER_TOR = 2
NUM_GPUS_PER_SERVER = 8

def count_combinations(x):
    # Number of groups with x gpus in them
    if (x % (NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER)) == 0:
        # just select ToRs as you fill up all their gpus
        tor_num = int(x / (NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER))
        comb = combination(NUM_TORS, tor_num)
        return comb
    
    if (x % (NUM_GPUS_PER_SERVER) == 0):
        # we fill up the gpus
        remaining_nodes = x % (NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER)
        group_num = 1
        tor_num = int(x / (NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER))
        comb = combination(NUM_TORS, tor_num)
        group_num *= comb
        comb = combination(NUM_TORS - tor_num, 1)
        group_num *= comb
        server_num = int (remaining_nodes / NUM_GPUS_PER_SERVER)
        comb = combination(NUM_SERVERS_PER_TOR, server_num)
        group_num *= comb
        return group_num

    # we partially use gpus of one server
    remaining_servers = x % (NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER)
    remaining_gpus = x % (NUM_GPUS_PER_SERVER)
    group_num = 1
    tor_num = int(x / (NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER))
    comb = combination(NUM_TORS, tor_num)
    group_num *= comb
    comb = combination(NUM_TORS - tor_num, 1)
    group_num *= comb
    server_num = int (remaining_servers / NUM_GPUS_PER_SERVER)
    comb = combination(NUM_SERVERS_PER_TOR, server_num)
    group_num *= comb
    comb = combination(NUM_SERVERS_PER_TOR - server_num, 1)
    group_num *= comb
    comb = combination(NUM_GPUS_PER_SERVER, remaining_gpus)
    group_num *= comb
    return group_num

min_gpu_num = 0
max_gpu_num = 128
maximized_locality_count = 0
naive_count = 0
for i in range(min_gpu_num, max_gpu_num+1):
    maximized_locality_count += count_combinations(i)
    naive_count += combination(NUM_TORS * NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER, i)

print(f'{NUM_GPUS_PER_SERVER} gpus per server, {NUM_SERVERS_PER_TOR} servers per tors, {NUM_TORS} tors')
print(f'Selecting groups of {min_gpu_num} - {max_gpu_num} nodes out of {NUM_TORS * NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER}')
print(f'naive: {naive_count}(2^{math.ceil(math.log2(naive_count))}), with maximized \
locality assumption: {maximized_locality_count}(2^{math.ceil(math.log2(maximized_locality_count))})')
print('---------------------')

maximized_locality_count = 0
naive_count = 0
gpu_num_list = [8, 16, 32, 64, 128]
for i in gpu_num_list:
    maximized_locality_count += count_combinations(i)
    naive_count += combination(NUM_TORS * NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER, i)

print(f'{NUM_GPUS_PER_SERVER} gpus per server, {NUM_SERVERS_PER_TOR} servers per tors, {NUM_TORS} tors')
print(f'Selecting groups of {gpu_num_list} nodes out of {NUM_TORS * NUM_SERVERS_PER_TOR * NUM_GPUS_PER_SERVER}')
print(f'naive: {naive_count}(2^{math.ceil(math.log2(naive_count))}), with maximized \
locality assumption: {maximized_locality_count}(2^{math.ceil(math.log2(maximized_locality_count))})')