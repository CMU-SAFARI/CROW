# Copy-row DRAM (CROW)

Source code of the simulators used to evaluate the mechanisms presented in
the ISCA'19 paper:
>H. Hassan, M. Patel, J. S. Kim, A. G. Yağlıkçı, N. Vijaykumar, N. Mansouri Ghiasi, S. Ghose, O. Mutlu.
>"[**CROW: A Low-Cost Substrate for Improving DRAM Performance, Energy Efficiency, and Reliability**](https://people.inf.ethz.ch/omutlu/pub/CROW-DRAM-substrate-for-performance-energy-reliability_isca19.pdf)".
>In _Proceedings of the International Symposium on Computer Architecture (ISCA)_, June 2019.


## 1. Evaluating CROW's Performance

Ramulator is a cycle-accurate memory simulator that support a wide array of
commercial and academic DRAM standards. We have modified the memory
controller of Ramulator to evaluate the performance of the *CROW-cache* and *CROW-ref* mechanisms
proposed in our paper.

### To build Ramulator, just run the following commands:
        $ cd ramulator
        $ make -j

### To start simulation with default CROW-cache and CROW-ref parameters, run:
        $ ./run.sh

Note that the script will run a very quick simulation using a small trace
file. Please refer to the [original Ramulator
repository](https://github.com/CMU-SAFARI/ramulator) for traces collected
from real workloads.

To change CROW configuration, you may either edit the configuration file
`configs/CROW_configs/LPDDR4.cfg` or edit the `run.sh` script to provide
command-line parameters when starting the *ramulator* executable. Here are some of
the key CROW command-line arguments:

* `copy_rows_per_SA` - the number of copy rows per subarray.
  `copy_rows_per_SA=0` disables both CROW-cache and CROW-ref
* `enable_crow_upperbound` - When `true`, it enables hypothetical CROW-cache
  with 100% hit rate
* `weak_rows_per_SA` - the number of weak regular rows per subarray.
  CROW-ref remaps these rows to the copy rows. The rest of the available
  copy rows can be utilized by CROW-cache. Set `copy_rows_per_SA` and
  `weak_rows_per_SA` to the same value to enable only CROW-ref but not
  CROW-cache.
* `refresh_mult` - refresh interval multiplier when CROW-ref is enabled.
  After remapping the weak rows, CROW-ref multiplies the default refresh
  interval by this parameter to reduce the DRAM refresh rate.

You can find all configuration parameters in `src/Config.h`.

You can execute `run_sweep_copy_rows.sh` to simulate CROW-cache by sweeping
the number of copy rows per subarray. This can be used to recreate the Figure 8
in the paper. The script uses traces collected from
a SPEC2006 workload, *bzip2*, as input workload. Edit the script to
simulate more workloads. You can find more workload traces in the original
Ramulator repository.

This repository also implements TL-DRAM and SALP, the two in-DRAM caching
mechanisms we compare CROW-cache against in the paper. We provide a bash
script, `run_crow_tl-dram_salp.sh`, which can be run simulate *bzip2* with
CROW-cache, TL-DRAM, and SALP configurations. The output can be used to
recreate Figure 11 in the paper.

Also, please refer to the [original Ramulator
repository](https://github.com/CMU-SAFARI/ramulator) for additional details
on Ramulator's source code.


## 2. Circuit-level Simulations

We use
[LTSpice](https://www.analog.com/en/design-center/design-tools-and-calculators/ltspice-simulator.html)
based DRAM circuit-level simulation to evaluate the latency of two-row (and
multiple-row) activation. We have used LTSpice on a Linux system using
[Wine](https://www.winehq.org/).

### Installation
1. If you are using a Linux system, first, install Wine. In Debian-based
  systems, you can use the following command:
        $ apt install wine
2. Download
   [LTSpice](https://www.analog.com/en/design-center/design-tools-and-calculators/ltspice-simulator.html) and follow the instructions to install.
3. Edit `LTSpice_executable_path` in file *SPICESim/LTspice-cli/config.py* to point
   to the LTSpice executable.
4. Run `make` inside the *SPICESim* directory to build the simple parser
   application used to process the raw output produced by LTSpice.

### Running LTSpice Simulation

For a quick run, you can just run the following command:
        
        $ ./run_sim.sh

This will perform circuit-level simulation to estimate tRCD/tRAS/tWR timing
parameters when simultaneously activating 1 to 8 rows that contain
the same data.
