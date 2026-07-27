import math

'''
Leaf spine topology

Spine set --> S = (s_1, s_2, ...)
Leaf set --> L = (l_1, l_2, ...)
Failure prob --> f

X_i = P(s_i not connected to all leafs) = 1 - P(s_i connected to all leafs) = 1 - (1 - f)^|L|
P(No spine connected to all leafs) = X_1 * X_2 * ... * X_|S| = (1 - (1 - f)^|L|)^|S|
'''

S = 16
LEAFS = 24
L = [1, 2, 4, 8, 16, 24]
F = [0.0001, 0.001, 0.01, 0.1, 0.5]
servers_per_leaf = 2
gpus_per_server = 8
print('LEAF SPINE')
for f in F:
    print(f'{S} spines, {LEAFS} leafs, {servers_per_leaf} servers per leaf, {gpus_per_server} GPUs per server, {f * 100.0}% failure probability\n')
    for l in L:
        if (l > LEAFS):
            raise Exception("Destination leafs greater than the total number of leafs")
        no_connectivity_prob = (1.0 - (1.0 - f)**l)**S
        print(f'{l} destination leaf(s), {l * servers_per_leaf * gpus_per_server} destination nodes')
        print(f'The probability that no spine is connected to all destination leafs is {no_connectivity_prob * 100.0}%\n')
    print('********************************')

'''
K-ary fattree topology

Core set --> C = (c1, c2, ...)
Destination Tor set --> T = (t_1, t_2, ...)
Destination pod number --> n = ceil(|T| / (K / 2.0))
P (destination tors accessible from C_i) = P(C_i connected to n pods) * P(C_i's corresponding agg in each pod connected to the destionation tors in that pod)
= (1-f)^n * (1-f)^|T| = (1-f)^(n+|T|)
P (no core connected to all destination tors with 2 hops) = (1 - (1-f)^(n+|T|))^|C|
'''
K = 8
T = [1, 2, 4, 8, 16, 32]
CORES = K**2 / 4.0
print('\n\nFAT-TREE')
for f in F:
    print(f'{CORES} cores, {K} pods, {K/2.0} aggs per pod, {K/2.0} edges per pod, {K/2.0} servers per edge, {gpus_per_server} GPUs per server, {f * 100.0}% failure probability\n')
    for t in T:
        if (t > K**2/2.0):
            raise Exception("Destination tors greater than the total number of tors")
        dst_pod_number = math.ceil(t / (K / 2.0))
        no_connectivity_prob = (1.0 - (1.0-f)**(dst_pod_number+t))**(CORES)
        print(f'{t} destination ToR(s), {dst_pod_number} destination pods, {t * (K/2.0) * gpus_per_server} destination nodes')
        print(f'The probability that no core is to hops away from all destination ToRs is {no_connectivity_prob * 100.0}%\n')
    print('********************************')