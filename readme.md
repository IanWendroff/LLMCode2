1. Open 4 terminals and ssh into different machines. Update network.cpp lines 119-122 to the IP addresses of the machines you're using. Update the port to whatever you want to use (1895. 4040, 4041, and 6000 to 6010 should all work). 

2. Run make

3. On the nodes run ```./my_program <node#> <#threads> <#operations>```, like so:

Node 0: ./my_program 0 3 1000 > node0.log 2>&1 & 
Means node 0, with 3 threads, running 1000 total operations (~333/thread)

Node 1: ./my_program 1 3 1000 > node1.log 2>&1 &

Node 2: ./my_program 2 3 1000 > node2.log 2>&1 &

Node 3: ./my_program 3 3 1000 > node3.log 2>&1 &

Change the number of threads and number of operations as you please. 

To change the keyranges, add or remove key ranges in main.cpp, line 135 (like in the commented out line above it). 

Make sure you run on the correct nodes (eg, "./my_program 0 3 1000" on the node at nodes[0] you specified in network.cpp)

You should be able to see standard output and errors from each node in the nodeX.log files that will be created 


4. These commands should execute correctly, with key_ranges = {100}:
Node 0: ./my_program 0 1 5 > node0.log 2>&1 & 

Node 1: ./my_program 1 1 5 > node1.log 2>&1 &

Node 2: ./my_program 2 1 5 > node2.log 2>&1 &

Node 3: ./my_program 3 1 5 > node3.log 2>&1 &

These should cause the nodes to hang and never complete:
Node 0: ./my_program 0 3 1000 > node0.log 2>&1 & 

Node 1: ./my_program 1 3 1000 > node1.log 2>&1 &

Node 2: ./my_program 2 3 1000 > node2.log 2>&1 &

Node 3: ./my_program 3 3 1000 > node3.log 2>&1 &

5. To stop the execution of the process run "pkill -f my_program"


If it runs to completion, the final output should be something like so in each node: 
"Node 3 finished key_range 100
Node 3 wrote to file
Node 3 closed file"

Then, csv files with performance metrics are created in src folder