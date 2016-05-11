# README #
This code is structured in several different functional blocks.

### Functional Blocks ###

* Staffetta Core
    * staffetta-test.c
    * staffetta.c/.h
    * staffetta_get_data.py
* Energytrace
* Metric

## What is the functionality of each block? ##

### Staffetta Core ###
####Staffetta.c/h
In these series of files is where the core of the communication protocol lies.
staffetta.c contains all the processes to communicate between nodes, manage wake ups and sleep periods
and where most of the data reporting is held (printf).

In staffetta.h all the defines and parameters are grouped. Parameters like number of initial packets
per node or which gradient is used appear there. More important, in this file is where the parameters
that allow us to switch between simulating original staffetta or the enhanced version for energy
harvesting reside.

Those parameters are the following:
* WITH_GRADIENT: Ensure that messages follows a gradient to the sink (number of wakeups)
* BCP_GRADIENT: Use the queue size as gradient (BCP)
* ORW_GRADIENT: Use the expected duty cycle as gradient (ORW)
* DYN_DC: Staffetta adaptative wakeups
* ENERGY_HARV: Enable dynamic budget depending on Energy Harvesting capabilities
* NEW_EDC: Uses modified computation of EDC accounting for node's state
* AGEING: EDC vector ages depending on rendezvous value
* STAFFETTA_ENERGEST: Uses eh modified energest consumption computation
* ELAPSED_TIME: Uses original ON time computation or modified
* ADAPTIVE_PACKET_CREATION: Packets are created when there is a change on Harvesting rate
* RANDOM_PACKET_CREATION: Packets are created randomly within a time window

####Staffetta-test.c
Is where the high level process calls are. In there initialization, operation energy
check or new packet creation reside.

####Staffetta_get_data.py
This script is in charge of collecting all the data located in the log and process it into useful informaiton
and graphs. In it its specified the paths to locate the files and the resulting names.

To invoke such file run is as: python staffetta_get_data.py <name_of_log> <number_of_logs>

A folder with the resulting processed data will be created and filled.

### Energytrace and Metric
A modified version of the original Energytrace developed by Xin to use energy harvesting with different energy models.
Energytrace has the goal to manage all the power variations of the different nodes in simulation. It provides or
reduces the energy of the nodes. It interfaces as well with the last group, Metric, which calculates simulation
parameters as Node State and Harvesting Rate.
Energytrace specifies when a node has had a change in harvesting rate and therefore should generate a new packet
if the mode has been enabled.
Harvesting Models
* MODEL_BERNOULLI: Probabilistic energy harvesting based on Bernoulli distribution
* MODEL_SOLAR / MODEL_MOVER: Node uses file with energy split in chunks of 10ms with either solar or
 mover energy located in its coffee file system. Depending on which file is loaded, the node will be Solar
 or a Mover powered.

Is in energytrace.h whrere parameters like initial battery value, max and min, acum_consumption and
acum_harvest can be checked and modified.

In metric.h the node state thresholds can be modified with: NS_ENERGY_LOW NS_ENERGY_MID and NS_ENERGY_HIGH	2461	//24.61mJ * 1000 (Floating Point)