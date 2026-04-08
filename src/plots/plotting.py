# import matplotlib.pyplot as plt

# Range of keys (x-axis)
# key_ranges = [1000, 10000, 100000]

# node_id,key_range,throughput_ops_sec,avg_latency_us,total_ops
# metrics_node_0_keyrange_1000.csv: 0,1000,8780.94,325.93,427
# metrics_node_0_keyrange_10000.csv: 0,10000,454.586,2394.55,484
# metrics_node_0_keyrange_100000.csv: 0,100000,10782.4,265.461,496
# metrics_node_1_keyrange_1000.csv: 1,1000,8411.96,340.213,428
# metrics_node_1_keyrange_10000.csv: 1,10000,10155.1,277.31,488
# metrics_node_1_keyrange_100000.csv: 1,100000,10298.6,281.056,498
# metrics_node_3_keyrange_1000.csv: 3,1000,9420.22,301.604,436
# metrics_node_3_keyrange_10000.csv: 3,10000,457.965,2387.28,485
# metrics_node_3_keyrange_100000.csv: 3,100000,10714.6,272.945,497
# metrics_node_2_keyrange_1000.csv: 2,1000,9431.46,297.572,441
# metrics_node_2_keyrange_10000.csv: 2,10000,10216,268.493,489
# metrics_node_2_keyrange_100000.csv: 2,100000,10438.7,270.639,496


import matplotlib.pyplot as plt
from collections import defaultdict

data = """metrics_node_2_keyrange_1000.csv: 2,1000,9431.46,297.572,441
metrics_node_0_keyrange_100000.csv: 0,100000,10782.4,265.461,496
metrics_node_1_keyrange_10000.csv: 1,10000,10108.2,282.574,487
metrics_node_3_keyrange_1000.csv: 3,1000,9420.22,301.604,436
metrics_node_3_keyrange_100000.csv: 3,100000,10714.6,272.945,497
metrics_node_3_keyrange_10000.csv: 3,10000,9317.64,298.292,485
metrics_node_0_keyrange_1000.csv: 0,1000,8780.94,325.93,427
metrics_node_1_keyrange_100000.csv: 1,100000,10298.6,281.056,498
metrics_node_2_keyrange_10000.csv: 2,10000,9117.51,310.259,481
metrics_node_1_keyrange_1000.csv: 1,1000,8411.96,340.213,428
metrics_node_2_keyrange_100000.csv: 2,100000,10438.7,270.639,496
metrics_node_0_keyrange_10000.csv: 0,10000,9480.66,297.341,486"""

# Dictionaries to collect values
throughput = defaultdict(list)
latency = defaultdict(list)

# Parse data
lines = data.strip().split("\n")[1:]  # skip header
for line in lines:
    _, values = line.split(": ")
    node_id, key_range, thr, lat, _ = values.split(",")
    
    key_range = int(key_range)
    thr = float(thr)
    lat = float(lat)
    
    throughput[key_range].append(thr)
    latency[key_range].append(lat)

# Compute averages
avg_throughput = {k: sum(v)/len(v) for k, v in throughput.items()}
avg_latency = {k: sum(v)/len(v) for k, v in latency.items()}

# Sort by key range
key_ranges = sorted(avg_throughput.keys())
avg_thr_values = [avg_throughput[k] for k in key_ranges]
avg_lat_values = [avg_latency[k] for k in key_ranges]

# Plot
fig, ax1 = plt.subplots()

# Throughput (left axis)
ax1.set_xlabel("Key Range")
ax1.set_ylabel("Avg Throughput (ops/sec)", color="blue")
ax1.plot(key_ranges, avg_thr_values, marker="o", color="blue", label="Throughput")
ax1.tick_params(axis='y', labelcolor="blue")

# Latency (right axis)
ax2 = ax1.twinx()
ax2.set_ylabel("Avg Latency (µs)", color="red")
ax2.plot(key_ranges, avg_lat_values, marker="s", color="red", label="Latency")
ax2.tick_params(axis='y', labelcolor="red")

plt.title("Average Throughput and Latency vs Key Range")
plt.grid(True)
plt.tight_layout()
plt.savefig("throughput_latency_vs_keys.png", dpi=300)
# plt.show()


# a0 = 12.6264+11.3089+11.2205
# a1 = 16.1204+11.9487+12.0633
# a2 = 15.422+11.9483+11.9045
# a3 = 15.5173+11.9468+11.8764

# b0 = (234.086+263.671+265.160)/3
# b1 = (182.591+247.863+246.682)/3
# b2 = (191.117 +248.069+246.986)/3
# b3 = (190.425+250.210+248.483)/3
# # Collected performance metrics
# # Replace these example values with real measurements
# average_throughput = [a0, a1, a2, a3]  # operations per second
# average_latency = [b0, b1, b2, b3]        # milliseconds per operation

# # -------- Plot 1: Throughput vs Range of Keys --------
# plt.figure(figsize=(8, 5))
# plt.plot(key_ranges, average_throughput, marker='o', linestyle='-')
# plt.xscale('log')
# plt.xlabel("Range of Keys")
# plt.ylabel("Average Throughput (ops/sec)")
# plt.title("System Throughput vs Range of Keys")
# plt.grid(True, which="both", linestyle="--", linewidth=0.5)
# plt.tight_layout()
# plt.savefig("throughput_vs_keys.png", dpi=300)
# plt.close()

# # -------- Plot 2: Latency vs Range of Keys --------
# plt.figure(figsize=(8, 5))
# plt.plot(key_ranges, average_latency, marker='s', linestyle='-', color='orange')
# plt.xscale('log')
# plt.xlabel("Range of Keys")
# plt.ylabel("Average Latency (ms)")
# plt.title("Average Latency vs Range of Keys")
# plt.grid(True, which="both", linestyle="--", linewidth=0.5)
# plt.tight_layout()
# plt.savefig("latency_vs_keys.png", dpi=300)
# plt.close()
