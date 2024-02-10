import random

packets_needed = [random.randint(1,200) for i in range(random.randint(1, 200))]
total_packets_need = sum(packets_needed)

ratio = [pn/total_packets_need for pn in packets_needed]

print(total_packets_need)
print(ratio)

max_packets = 1000

assigned = [1 for i in range(len(packets_needed))]





