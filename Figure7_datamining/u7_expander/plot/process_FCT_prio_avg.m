% process data:

clear;clc;

perc_load=1;
Nruns=4;

data=csvread('../../../Figure1_traffic/datamining.csv');
flowsize=data(:,1).';
% flowcdf=data(:,2).';
Nflowsize=length(flowsize);

FCTs99_avg=zeros(1,Nflowsize);

matname=sprintf('FCT_%dperc_VLB0_prio_avg.mat',perc_load);
savemat=1;

for run=1:Nruns
    
    load(sprintf('FCT_%dperc_VLB0_prio_%d.mat',perc_load,run));
    
    Nrecorded_flows=length(data_fct(:,1));
    Nfiltered_flows=zeros(1,Nflowsize);
    for fs=1:Nflowsize
        fprintf('iter_fs = %d out of %d\n',fs,Nflowsize);
        for i=1:Nrecorded_flows
            if data_fct(i,3)==flowsize(fs)
                Nfiltered_flows(fs)=Nfiltered_flows(fs)+1;
            end
        end
    end
    filtered_flows=cell(1,Nflowsize);
    FCTs99=zeros(1,Nflowsize);
    for fs=1:Nflowsize
        filtered_flows{fs}=zeros(Nfiltered_flows(fs),5);
    end
    for fs=1:Nflowsize
        fprintf('iter_fs = %d out of %d\n',fs,Nflowsize);
        cnt=1;
        for i=1:length(data_fct(:,1))
            if data_fct(i,3)==flowsize(fs)
                filtered_flows{fs}(cnt,:)=data_fct(i,:);
                cnt=cnt+1;
            end
        end
        if isempty(filtered_flows{fs})==0
            perc=.99; % percentile
            temp=sort(filtered_flows{fs}(:,4));
            [n,~]=size(temp);
            FCTs99(fs)=temp(round(perc*n));
            
        end
    end
    FCTs99_avg=FCTs99_avg+FCTs99;
end

FCTs99_avg=FCTs99_avg/Nruns;

figure;
hold on;
plot(flowsize,1e3*FCTs99_avg,'o','linewidth',2);
grid on;
box on;
ax=gca;
ax.XScale='log';
ax.YScale='log';
ax.FontSize=16;
xlabel('Flow size (bytes)');
ylabel('Mean FCT (\mus)');
% plot(17*[1e6 1e6],ax.YLim,'--r');
xlim([100 1e9]);
ax.XTick=[10^2 10^4 10^6 10^8];
ylim([1 10^6]);

if savemat == 1
    save(matname,'FCTs99_avg');
end

%%









