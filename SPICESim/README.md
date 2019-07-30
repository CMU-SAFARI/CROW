TODO: format this README better


Configuration: 

Set LTSpice executable path in LTspice-cli/config.py.
Example in Linux systems with wine: 
LTSpice_executable_path = 'wine ~/.wine/drive_c/Program\ Files/LTC/LTspiceXVII/XVIIx64.exe'

Original LTspice-cli repo: https://github.com/joskvi/LTspice-cli


Usage: 
    make

    "make" build the "parser" program which analyzes raw output data of LTSpice

    ./run_sim.sh

This script will run SPICE simulations to evaluate tRCD/tRAS/tWR latencies when
simultaneously activating [1-8] cells.

Output file format: \*.out files in the output directory contain min/max tRCD/tRAS/tWR
values (in nanoseconds) measures over 100 MonteCarlo iterations. The output
file also shows how many times the expected outcome was obtained during the
iterations.

Example output:
min_trcd, 6.44099
max_trcd, 7.53083
num_correct_trcd, 100 // the expected data was read correctly during all of the 100 iterations
min_tras, 26.8575
max_tras, 30.9424
num_correct_tras, 100 // the expected data was restored to the cell(s) correctly during all of the 100 iterations
min_twr, 16.2776
max_twr, 19.2947
num_correct_twr, 100 // a new value was written to the cell(s) correctly during all of the 100 iterations


