
% Reload python module
testing = 1;
if testing
    clear classes;  
    mod = py.importlib.import_module('analysis_tools');
    py.reload(mod);
end

% Analysis parameters
save_plots  = 0;
file = 'data/file.csv';

% Read raw data
fprintf([file, '\n'])
data = importdata(file, ',', 17);
t = data.data(:,1);
v_ds = data.data(:,2);
i_ds = data.data(:,4);

% Filter raw data
sr = length(t)/max(t);
ff = 100e6;
wn = ff/(sr*0.5);
[B,A] = butter(2, wn);
v_ds = filter(B, A, v_ds);
i_ds = filter(B, A, i_ds);
% Determine intervals
start = find(t>-5e-7);
start = start(1);
stop = find(t>7.5e-6);
stop = stop(1);
t = t(start:stop) - t(start);
v_ds = v_ds(start:stop);
snubber_voltage = snubber_voltage(start:stop);
i_ds = i_ds(start:stop);
% Remove current bias
current_bias = mean(i_ds(i_ds < 4));
i_ds = i_ds - current_bias;
% Set nominal levels
nominal_voltage = 600;
nominal_current = mean(i_ds(1:100));

% Analyse

fft_frequency_peak = get_fft(t, v_ds, save_plots, file);
fprintf('FFT gives peak at frequency %4.1f MHz\n', fft_frequency_peak * 1e-6)

analyzed_data = py.analysis_tools.analyze_data_single(t', i_ds', v_ds', nominal_current, nominal_voltage, snubber_voltage', snubber_resistance);
time_indexes = analyzed_data{1};
time_indexes = cellfun(@int64, cell(time_indexes));

fprintf('Voltage rise time:\t\t%4.1f ns\n', analyzed_data{2});
fprintf('Voltage fall time:\t\t%4.1f ns\n', analyzed_data{3});
fprintf('Current rise time:\t\t%4.1f ns\n', analyzed_data{4});
fprintf('Current fall time:\t\t%4.1f ns\n', analyzed_data{5});
fprintf('Turn-on loss:\t\t\t%5.3f mJ\n', analyzed_data{6});
fprintf('Snubber turn-on loss:\t%5.3f mJ\n', analyzed_data{12});
fprintf('Turn-off loss:\t\t\t%5.3f mJ\n', analyzed_data{7});
fprintf('Snubber turn-off loss:\t%5.3f mJ\n', analyzed_data{13});
fprintf('Voltage overshoot:\t\t%4.1f V\n', analyzed_data{8});
fprintf('Max current:\t\t\t%4.1f A\n', analyzed_data{9});
fprintf('Ringing frequency:\t\t%4.1f MHz\n', analyzed_data{10});
fprintf('Damping ratio:\t\t\t%4.1f M 1/s\n', analyzed_data{11});

% Make plots

time = t* 10^6; % Change to microseconds [us]
time_interval_1 = [0 3];
time_interval_2 = [5 8];
make_plots(time, time_interval_1, time_interval_2, v_ds, i_ds, save_plots, file);
