
loads=[1 5 10];
Nloads=length(loads);

fcts=cell(1,Nloads);

% for a=1:Nloads
%     load(sprintf('FCT_%dperc.mat',loads(a)));
%     fcts{a}=data_fct;
% end
for a=1:Nloads
    load(sprintf('FCT_%dperc_5paths.mat',loads(a)));
    fcts{a}=data_fct;
end

data=csvread('../../../Figure1_traffic/websearch.csv');
flowsize=data(:,1).';
Nflowsize=length(flowsize);

meanFCTs=cell(1,Nloads);
FCTs99=cell(1,Nloads);

for a=1:Nloads
    Nrecorded_flows=length(fcts{a});
    
    % get the number of flows in each size
    Nfiltered_flows=zeros(1,Nflowsize);
    for fs=1:Nflowsize
        for i=1:Nrecorded_flows
            if fcts{a}(i,3)==flowsize(fs)
                Nfiltered_flows(fs)=Nfiltered_flows(fs)+1;
            end
        end
    end
    
    % filter the flows by size
    filtered_flows=cell(1,Nflowsize);
    [meanFCTs{a},FCTs99{a}]=deal(zeros(1,Nflowsize));
    for fs=1:Nflowsize
        filtered_flows{fs}=zeros(Nfiltered_flows(fs),5);
    end
    for fs=1:Nflowsize
        fprintf('iter_fs = %d out of %d\n',fs,Nflowsize);
        cnt=1;
        for i=1:length(fcts{a}(:,1))
            if fcts{a}(i,3)==flowsize(fs)
                filtered_flows{fs}(cnt,:)=fcts{a}(i,:);
                cnt=cnt+1;
            end
        end
        if isempty(filtered_flows{fs})==0
            meanFCTs{a}(fs)=mean(filtered_flows{fs}(:,4));
            perc=.99; % percentile
            temp=sort(filtered_flows{fs}(:,4));
            [n,~]=size(temp);
            FCTs99{a}(fs)=temp(round(perc*n));
        end
    end
    
end

%%

figure;
hold on;

h(1)=plot(flowsize,1e3*meanFCTs{1},'o','linewidth',2);
plot(flowsize,1e3*meanFCTs{1},'-k','linewidth',1,'color',h(1).Color);
for a=2:3
    h(a)=plot(flowsize,1e3*FCTs99{a},'o','linewidth',2);
    plot(flowsize,1e3*FCTs99{a},'-k','linewidth',1,'color',h(a).Color);
end

plot(flowsize,1e3*1e3*(flowsize+64)*8/10e9,'--k','linewidth',1);

grid on;
box on;
ax=gca;
ax.XScale='log';
ax.YScale='log';
ax.FontSize=16;
xlabel('Flow size (bytes)');
ylabel('Flow completion time (\mus)');
xlim([4000 30e6]);
ax.XTick=[10^4 10^5 10^6 10^7];
ylim([10^1 10^5]);

hleg=legend(h,'1% load, avg.','5% load, 99%-tile', ...
    '10% load, 99%-tile');
hleg.FontSize=14;
hleg.Location='northwest';

ht = text(5e4,1e1,'Serialization latency');
set(ht,'Rotation',39)
set(ht,'FontSize',14)



