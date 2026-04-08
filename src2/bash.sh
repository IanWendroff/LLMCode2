# Run these commands on the matching node machine (0..3), as in readme.md

# Scenario A (should complete with key_ranges={100})
# Node 0: ./my_program 0 1 5 > node0.log 2>&1 &
# Node 1: ./my_program 1 1 5 > node1.log 2>&1 &
# Node 2: ./my_program 2 1 5 > node2.log 2>&1 &
# Node 3: ./my_program 3 1 5 > node3.log 2>&1 &

# Scenario B (high-contention debug case from readme)
# Node 0: ./my_program 0 3 1000 > node0.log 2>&1 &
# Node 1: ./my_program 1 3 1000 > node1.log 2>&1 &
# Node 2: ./my_program 2 3 1000 > node2.log 2>&1 &
# Node 3: ./my_program 3 3 1000 > node3.log 2>&1 &

# To stop:
# pkill -f my_program