import numpy as np
import config

# ------- Control functions ------- #

def make_report(file_paths):
    out = open(config.output_data_path + config.output_data_summary_filename, 'w+')
    for file_path in file_paths:
        file_path = file_path[:-4] + '_analysis.txt'
        f = open(file_path)
        lastline = None
        for l in f:
            lastline = l
        out.write(file_path + '\t' + lastline)
        f.close()
    out.close()

def read_simulation_output(file_path):
    # Read data from file
    f = open(file_path)
    data=[]
    header=[]
    # Get lines
    for line_num, line in enumerate(f):
        if line_num > 1:
            data.append([float(num) for num in line.split()])
        else:
            header.append(line.split())
    f.close()
    # Create numpy array
    data = np.array(data, dtype=float)
    return (header, data)

def analyze_data(file_path):

    # Read data
    header, data = read_simulation_output(file_path)

    # Pick and define relevant data
    # Remove inital data, until 40 us
    start_data = np.where(data[:,0] > 40*10**(-6))[0][0]
    data = data[start_data:,:]
    data = data[:,0:7]
    data[:,1] = data[:,1] - data[:,4] # Voltage accros upper mosfet
    # Header info
    header_variables = header[1]
    params = header[0]
    # Circuit variables
    snubber_resistance = [param.split('=')[1] for param in params if ('R_s' in param.split('='))][0]
    snubber_resistance = float(snubber_resistance.split(',')[0])
    output_current = [param.split('=')[1] for param in params if ('I_out' in param.split('='))][0]
    output_current = float(output_current.split(',')[0])
    input_voltage = 600
    time = data[:, 0]
    # Final output list
    all_output_info = []

    # Open file to print
    f = open(file_path[:-4] + '_analysis.txt', 'w+')
    f.write('Variables: ' + ', '.join(header_variables) + '\n')
    f.write(' '.join(params[3:10]) + '\n' + ' '.join(params[10:]) + '\n')

    # Perform analysis for upper and lower part of half-bridge
    for i in range(len(data[0,1:])/3):
        col = 3*i + 1

        # Find voltage switching times
        rise_v, fall_v, rise_found, fall_found = calculate_switching_times(data[:, col], input_voltage)
        if rise_found:
            rise_time_voltage = (time[rise_v[0]], time[rise_v[1]])
        else:
            rise_time_voltage = (0,0)
        if fall_found:
            fall_time_voltage = (time[fall_v[0]], time[fall_v[1]])
        else:
            print time[fall_v[0]]
            print time[fall_v[1]]
            fall_time_voltage = (0,0)

        # Find current switching times
        rise_i, fall_i, rise_found, fall_found = calculate_switching_times(data[:, col+1], output_current)
        if rise_found:
            rise_time_current = (time[rise_i[0]], time[rise_i[1]])
        else:
            rise_time_current = (0,0)
        if fall_found:
            fall_time_current = (time[fall_i[0]], time[fall_i[1]])
        else:
            fall_time_current = (0,0)

        # Get overshoots
        voltage_undershoot, voltage_overshoot = calculate_overshoots(data[:, col])
        current_undershoot, current_overshoot = calculate_overshoots(data[:, col+1])
        max_current = max([abs(current_overshoot), abs(current_undershoot)])

        # Calculate ringing and damping ratio
        try:
            ringing_freq, decay_ratio = calculate_ringing(time, data[:, col], input_voltage)
            period = np.where(time < time[0] + 1/ringing_freq)[0][-1]
        except IndexError:
            ringing_freq = 0
            decay_ratio = 0
            period = 50
        ringing_freq = ringing_freq / 10**6 # [MHz]
        decay_ratio = decay_ratio / 10**6 # [MHz]

        # Calculate turn-on power loss
        start = min([rise_i[0], fall_v[0]])
        stop = max([rise_i[1], fall_v[1]])
        start -= 2*(stop-start)
        delta = 0
        while True:
            delta += 100
            if max(data[stop+delta:stop+delta+3*period, col+1]) < output_current*1.05:
                break
        stop+=delta
        E_turnon = calc_switch_loss(time[start:stop], data[start:stop, col], data[start:stop, col+1]) *10**3 #[mJ]
        E_turnon_snubber = np.multiply(snubber_resistance, calc_switch_loss(time[start:stop], data[start:stop, col+2], data[start:stop, col+2])) *10**3 #[mJ]

        # Calculate turn-off power loss
        start = min([fall_i[0], rise_v[0]])
        stop = max([fall_i[1], rise_v[1]])
        start -= 2*(stop-start)
        delta = 0
        while True:
            delta += 100
            if max(data[stop+delta:stop+delta+3*period, col]) < input_voltage*1.05:
                break
        stop+=delta
        E_turnoff = calc_switch_loss(time[start:stop], data[start:stop, col], data[start:stop, col+1]) *10**3 #[mJ]
        E_turnoff_snubber = np.multiply(snubber_resistance, calc_switch_loss(time[start:stop], data[start:stop, col+2], data[start:stop, col+2])) *10**3 #[mJ]


        # Write outdata
        f.write(['\n------------- Upper transistor ------------- \n\n', '\n------------- Lower transistor ------------- \n\n'][i])
        rise_time_voltage = (rise_time_voltage[1] - rise_time_voltage[0]) * 10**9 # [ns]
        fall_time_voltage = (fall_time_voltage[1] - fall_time_voltage[0]) * 10**9 # [ns]
        rise_time_current = (rise_time_current[1] - rise_time_current[0]) * 10**9 # [ns]
        fall_time_current = (fall_time_current[1] - fall_time_current[0]) * 10**9 # [ns]
        all_output_info.extend([rise_time_voltage, fall_time_voltage, rise_time_current, fall_time_current])
        all_output_info.extend([E_turnon, E_turnoff, E_turnon_snubber, E_turnoff_snubber])
        all_output_info.extend([voltage_overshoot, max_current])
        all_output_info.extend([ringing_freq, decay_ratio])
        f.write('Voltage Rise time [ns]: ' + str(rise_time_voltage) + '\n')
        f.write('Voltage Fall time [ns]: ' + str(fall_time_voltage) + '\n')
        f.write('Current Rise time [ns]: ' + str(rise_time_current) + '\n')
        f.write('Current Fall time [ns]: ' + str(fall_time_current) + '\n')
        f.write('Turn-on loss [mJ]: ' + str(E_turnon) + '\n')
        f.write('Turn-off loss [mJ]: ' + str(E_turnoff) + '\n')
        f.write('Turn-on loss snubbers [mJ]: ' + str(E_turnon_snubber) + '\n')
        f.write('Turn-off loss snubbers [mJ]: ' + str(E_turnoff_snubber) + '\n')
        f.write('Voltage overshoot [V]: ' + str(voltage_overshoot) + '\n')
        f.write('Largest current [A]: ' + str(max_current) + '\n')
        f.write('Ringing frequency [MHz]: ' + str(ringing_freq) + '\n')
        f.write('Damping ratio [M 1/s]: ' + str(decay_ratio) + '\n')

    # Write all info on one final line
    f.write('\n')
    f.write('\t'.join([str(i) for i in all_output_info]) + '\n')

    # Close file
    f.close()


# ------- Analysis functions ------- #

def local_extrema(data):
    local_max = np.where(np.r_[True, data[1:] > data[:-1]] & np.r_[data[:-1] > data[1:], True])[0]
    local_min = np.where(np.r_[True, data[1:] < data[:-1]] & np.r_[data[:-1] < data[1:], True])[0]
    return (local_min, local_max)

def calculate_switching_times(data, nominal_value):

    local_min, local_max = local_extrema(data)
    extrema = np.concatenate([local_min, local_max])
    extrema = np.sort(extrema)

    rise_time_found =  False
    fall_time_found = False

    prev_x = extrema[0]
    for index, x in enumerate(extrema[1:]):
        if data[prev_x] < 0.1 * nominal_value and data[x] > 0.9 * nominal_value:
            start_rise = prev_x + np.where(data[prev_x:x] > 0.1*nominal_value)[0][0]
            stop_rise = prev_x + np.where(data[prev_x:x] > 0.9*nominal_value)[0][0]
            rise = (start_rise, stop_rise)
            rise_time_found = True

        if data[prev_x] > 0.9 * nominal_value and data[x] < 0.1 * nominal_value:
            start_fall = prev_x + np.where(data[prev_x:x] < 0.9*nominal_value)[0][0]
            stop_fall = prev_x + np.where(data[prev_x:x] < 0.1*nominal_value)[0][0]
            fall = (start_fall, stop_fall)
            fall_time_found = True

        if rise_time_found and fall_time_found:
            break
        prev_x = x

    if not rise_time_found:
        prev_x = extrema[0]
        for index, x in enumerate(extrema[1:]):
            if data[prev_x] < 0.4 * nominal_value and data[x] > 0.6 * nominal_value:
                start_rise = np.where(data[:x] < 0.1*nominal_value)[0][-1]
                stop_rise = prev_x + np.where(data[prev_x:] > 0.9*nominal_value)[0][0]
                rise = (start_rise, stop_rise)
            prev_x = x
    if not fall_time_found:
        prev_x = extrema[0]
        for index, x in enumerate(extrema[1:]):
            if data[prev_x] > 0.6 * nominal_value and data[x] < 0.4 * nominal_value:
                start_fall = np.where(data[:x] > 0.9*nominal_value)[0][-1]
                stop_fall = prev_x + np.where(data[prev_x:] < 0.1*nominal_value)[0][0]
                fall = (start_fall, stop_fall)
            prev_x = x

    try:
        return (rise, fall, True, True) # Return
    except UnboundLocalError:
        # Some value not found
        print 'Error: unable to find rise or fall time. Zero is returned.'
        return ((0,0), (0,0), False, False)

def calc_switch_loss(time, voltage, current):
    E = 0
    for i in range(len(voltage)-1):
        E += (time[i+1] - time[i]) / 2.0 * (voltage[i]*current[i] + voltage[i+1]*current[i+1])
    return E

def calculate_overshoots(data):
    return (min(data), max(data))

def calculate_ringing(time, data, nominal_value):
    # Ringing frequency calculation
    number_of_peaks = 5
    peak = max(data)
    first_peak = np.where(data == peak)[0][0]
    index_of_peaks = local_extrema(data[first_peak:])[1]
    index_of_peaks = index_of_peaks[0:number_of_peaks] # Pick the first peaks, as defined
    index_of_peaks = np.add(index_of_peaks, first_peak) # Shift peaks to correct indexes
    peak_times = time[index_of_peaks]
    time_diff = [peak_times[i+1] - peak_times[i] for i in range(number_of_peaks-1)]
    ringing_freq = 1/np.mean(time_diff) # Calculate frequency based on avg time diff between peaks
    # Dacay ratio calculation
    voltage_peaks = data[index_of_peaks] - nominal_value
    decay_ratios = []
    for i in range(number_of_peaks - 1):
        alpha = np.log(voltage_peaks[i+1] / voltage_peaks[i]) / time_diff[i]
        decay_ratios.append(alpha)
    decay_ratio = (-1) * np.mean(decay_ratios)
    return (ringing_freq, decay_ratio)

def calculate_switching_times_alternative(data, nominal_value):

    going_up = False
    going_down = False

    fall = [0,0]
    rise = [0,0]
    found_rise = False
    found_fall = False

    data_point_prev = data[0]
    for step, data_point in enumerate(data[1:]):
        if data_point > 0.1 * nominal_value and data_point_prev < 0.1 * nominal_value:
            if not going_up:
                rise[0] = step
            going_up = True
            going_down = False
        if data_point > 0.9 * nominal_value and data_point_prev < 0.9 * nominal_value:
            if going_up:
                rise[1] = step
                found_rise = True
            if going_down:
                rise[0] = 0
            going_up = False
            going_down = False
        if data_point < 0.9 * nominal_value and data_point_prev > 0.9 * nominal_value:
            if not going_down:
                fall[0] = step
            going_up = False
            going_down = True
        if data_point < 0.1 * nominal_value and data_point_prev > 0.1 * nominal_value:
            if going_down:
                fall[1] = step
                found_fall = True
            if going_up:
                fall[0] = 0
            going_up = False
            going_down = False

        if found_fall and found_rise:
            break

        data_point_prev = data_point

    return (rise, fall)
