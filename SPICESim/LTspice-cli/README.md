# LTspice-cli

## About

LTspice-cli is a command line tool, which enables running simulations in LTspice from the command line. It also has the ability to specify a file containing the circuit parameters which the simulations should be run with. LTspice-cli is written in Python 2.7, and requires NumPy to run. The tool is only tested with LTspice IV.

## How to use

After downloading the files, the config.py file must be edited. Once configurated, a simulation can be run using
```
python run.py -r
```
To specify a file containing parameters, use
```
python run.py -f <inputfile>
```
Use option -h to see help in the command line.

## Parameter file

A file can be used to set the parameters in a circuit simulation. A parameter must first be set in LTspice, for example a resistor:

```
.param R=500
```
To change a parameter at run time, the `set` keyword in the parameter file can be used as follows:
```
set R 1k
```
The resistor R will now be set to 1k before running the simulation. To set a range of variables to be used in simulation, the `run` keyword is used. To simulate with the values 1, 100 and 1k for a capacitor C, the following line would be used:
```
run C 1 100 1k
```
Both `set` and `run` can be used in the same parameter file. Using the `set` command alone will not run a simulation, only change the LTspice file. To comment a line, begin the line with `#`.
