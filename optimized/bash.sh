# Node 0
#node#, #threads, #operations
./my_program 0 3 1000 > node0.log 2>&1 &
#later change back to 100000 operations

# wait 1 second for Node 0 server to start
sleep 1

# Node 1
./my_program 1 3 1000 > node1.log 2>&1 &

# wait 1 second for Node 1
sleep 1

# Node 2
./my_program 2 3 1000 > node2.log 2>&1 &

# # wait 1 second for Node 1
# sleep 1

# # Node 3
# ./my_program 3 1 100 > node3.log 2>&1 &
./my_program 3 3 1000 > node3.log 2>&1 &

tail -f node0.log node1.log node2.log node3.log