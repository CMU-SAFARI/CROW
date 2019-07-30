function [voltage_interval, current_interval] = find_axis_limits(time, time_interval, current, volt)

    % Axis are globally equal, and zero levels align horizontally
    time_interval_1 = time_interval(1,:);
    time_interval_2 = time_interval(2,:);
    t_1 = find(time >= time_interval_1(1) & time <= time_interval_1(2));
    t_2 = find(time >= time_interval_2(1) & time <= time_interval_2(2));
    voltage_interval = sort([volt(t_1(1)), volt(t_2(1))]);
    current_interval = sort([current(t_1(1)), current(t_2(1))]);

    scale_up_voltage = max(volt)/voltage_interval(end);
    scale_down_voltage = min(volt)/voltage_interval(end);
    scale_up_current = max(current)/current_interval(end);
    scale_down_current = min(current)/current_interval(end);

    voltage_interval = voltage_interval(end) - voltage_interval(1);
    voltage_interval = [voltage_interval*scale_down_voltage, voltage_interval*scale_up_voltage];
    current_interval = current_interval(end) - current_interval(1);
    current_interval = [current_interval*scale_down_current, current_interval*scale_up_current];
    voltage_ratio = abs(max(voltage_interval) / min(voltage_interval));
    current_ratio = abs(max(current_interval) / min(current_interval));

    if abs(min(voltage_interval)) < 5 || voltage_ratio > current_ratio
        voltage_interval(1) = -voltage_interval(2) / current_ratio;
    else
        current_interval(1) = -current_interval(2) / voltage_ratio;
    end

    voltage_margin = max(abs(voltage_interval))*0.05;
    current_margin = max(abs(current_interval))*0.05;
    voltage_interval = [voltage_interval(1)-voltage_margin voltage_interval(end)+voltage_margin];
    current_interval = [current_interval(1)-current_margin current_interval(end)+current_margin];

end
