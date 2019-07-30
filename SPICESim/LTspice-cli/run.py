import sys, getopt
import simulation_tools
import config
try:
    import analysis_tools
except ImportError:
    pass


def simulate(filename=None, do_analysis=False):

    asc_file_path = config.LTSpice_asc_filename

    if filename is not None:
        # Parse the file containing the parameters
        command_list = simulation_tools.parse_parameter_file(filename)

        # Run the list of commands
        number_of_finished_simulations = 0
        all_filenames = []
        if command_list is None:
            print('Syntax error in parameter file.')
            return
        for command in command_list:
            if command[0] == 's':
                parameter = command[1]
                value = command[2]
                # Set parameter as specified
                print('Setting parameter:  ' + str(parameter) + '=' + str(value))
                simulation_tools.set_parameters(asc_file_path, parameter, value, True)
            if command[0] == 'r':
                parameter = command[1]
                parameter_values = command[2]
                # Run tests with the given parameter and corresponding list of parameter values
                # The filenames of the output data is returned
                filenames = simulation_tools.run_simulations([parameter, parameter_values], number_of_finished_simulations)
                all_filenames.extend(filenames)
                number_of_finished_simulations += len(parameter_values)
                if do_analysis:
                    analyze(filenames)
        
        simulation_tools.run_simulations()

        # If analysis made, make a report with all the analysied data
        if do_analysis:
            analysis_tools.make_report(all_filenames)
    else:
        # No parameter file is specified, run simulations with defaults values
        simulation_tools.run_simulations()

def analyze(filenames):
    # Any analysis to be done on the simulation results can be be coded here.
    for filename in filenames:
        analysis_tools.analyze_data(filename)

def help():
    print 'auto.py -r -f <parameterFile> -a\nUsing the option -a to analyze, requires -f to be set'

def main(argv):

    # Get arguments
    try:
        opts, args = getopt.getopt(argv, 'hrf:m:o:a', ['file=', 'run'])
    except getopt.GetoptError:
        help()
        sys.exit(2)

    # Parse arguments
    parameter_file = None
    do_analysis = False
    for opt, arg in opts:
        if opt == '-h':
            help()
            sys.exit()
        elif opt in ('-f', '--file'):
            parameter_file = arg
        elif opt in ('-a'):
            do_analysis = True
        elif opt in ('-m'):
            config.LTSpice_asc_filename = arg
        elif opt in ('-o'):
            config.output_data_path = arg
        elif opt in ('-r', '--run'):
            simulate()
            sys.exit()

    # Run simulations based on arguments
    if parameter_file is not None:
        simulate(parameter_file, do_analysis)

if __name__ == '__main__':
    main(sys.argv[1:])
