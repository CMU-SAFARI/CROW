function make_plots(time, time_interval_1, time_interval_2, volt, current, save_plots, file_name)
    

        %% Start making plots
        hold off
        [voltage_interval, current_interval] = find_axis_limits(time, [time_interval_2; time_interval_1], current, volt);
        
        %% Plot turn-off

        % Find plot intervals
        time_interval = time_interval_1;

        % Create plots
        hFig = figure(1);
        set(hFig, 'Position', [100 100 800 600])
        set(gca,'FontSize',16)
        set(findall(gcf,'type','text'),'FontSize',16)
        grid on

        xlim(time_interval)
        xlabel('Time [us]')

        yyaxis left
        plot(time, volt, 'LineWidth', 1)
        ylim(voltage_interval)
        ylabel('Voltage [V]')

        yyaxis right
        plot(time, current, 'LineWidth', 1)
        ylim(current_interval)
        ylabel('Current [A]')
        
        if save_plots
        set(gcf, 'PaperPosition', [-0.1 0 8 6]);
        set(gcf, 'PaperSize', [8 6]);
        pdfname = [file_name(1:end-4), '_turnoff']; 
        saveas(gcf, pdfname, 'pdf')
        end

        %% Plot turn-on

        % Find plot intervals
        time_interval = time_interval_2;

        % Create plots
        hFig = figure(2);
        set(hFig, 'Position', [400 100 800 600])
        set(gca,'FontSize',16)
        set(findall(gcf,'type','text'),'FontSize',16)
        grid on

        xlim(time_interval)
        xlabel('Time [us]')

        yyaxis left
        plot(time, volt, 'LineWidth', 1)
        ylim(voltage_interval)
        ylabel('Voltage [V]')

        yyaxis right
        plot(time, current, 'LineWidth', 1)
        ylim(current_interval)
        ylabel('Current [A]')
        
        if save_plots
        set(gcf, 'PaperPosition', [-0.1 0 8 6]);
        set(gcf, 'PaperSize', [8 6]);
        pdfname = [file_name(1:end-4), '_turnon']; 
        saveas(gcf, pdfname, 'pdf')
        end
end