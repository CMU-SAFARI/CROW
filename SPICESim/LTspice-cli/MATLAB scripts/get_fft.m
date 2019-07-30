function f_peak = get_fft(time, data, save_plots, file_name)

    % Fast Fourier transform
    L = length(time);
    time_step = (time(end)-time(1))/L;
    Fs = 1/time_step;

    NFFT = 2^nextpow2(L);
    Y = fft(data,NFFT)/L;
    f = Fs/2*linspace(0,1,NFFT/2+1);
    abs_y = 2*abs(Y(1:NFFT/2+1));
    abs_y = 20*log(abs_y);
    
    % Find local peak
    db_peak = max(abs_y(f > 1e7 & f < 1e8));
    f_peak = f(abs_y == db_peak);

    % Plots
    hFig = figure(4);
    semilogx(f,abs_y, 'LineWidth', 1)
    grid on
    
    xlim([0  3.5e8])
    xlabel('Frequency [Hz]')
    ylabel('Amplitude [dB]')
    
    set(hFig, 'Position', [100 100 800 600])
    set(gca,'FontSize',16)
    set(findall(gcf,'type','text'),'FontSize',16)
    
    if save_plots
    set(gcf, 'PaperPosition', [-0.1 0 8 6]);
    set(gcf, 'PaperSize', [8 6]);
    pdfname = [file_name(1:end-4), '_fft']; 
    saveas(gcf, pdfname, 'pdf')
    end

end