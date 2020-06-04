% process data:

perc_load=1;

fname=sprintf('../sim/FCT_combined_cwnd20_%dperc_5paths_1hop.txt',perc_load);
matname=sprintf('FCT_%dperc_5paths.mat',perc_load);


fid=fopen(fname,'r');
string=fgetl(fid);
cnt_line=0;
cnt_fct=0;
cnt_util=0;
while ischar(string)
    if strcmp(string(1:3),'FCT')
        cnt_line=cnt_line+1;
        cnt_fct=cnt_fct+1;
        data_fct(cnt_fct,:)=sscanf(string,'%*s %d %d %d %f %f %*\n').';
        % src, dst, bytes, fct, time_started
        string=fgetl(fid);
    elseif strcmp(string(1:4),'Util')
        cnt_line=cnt_line+1;
        cnt_util=cnt_util+1;
        data_util(cnt_util,:)=sscanf(string,'%*s %f %f %*\n').';
        string=fgetl(fid);
    else
        cnt_line=cnt_line+1;
        %             fprintf(sprintf('Load = %.2f, data format mismatch\n',Load1(lind)));
        %             fprintf(sprintf('   >> line = %d\n',cnt_line));
        string=fgetl(fid);
    end
end
fclose(fid);

Nrecorded_flows=length(data_fct(:,1));

% ---

figure;
hold on;
plot(data_util(:,2),data_util(:,1),'-o','linewidth',2);
ylim([0 1]);
ax=gca;
ax.FontSize=16;
xlabel('Time (ms)');
ylabel('Utilization & traffic density');
grid on;
box on;
xlim([data_util(1,2) data_util(end,2)]);

% times=(10:10:200);
% traff_densities=zeros(1,20);
% cnt=1;
% active_conns=ones(648);
% for a=1:108
%     active_conns((a-1)*6+1:a*6,(a-1)*6+1:a*6)=zeros(6);
% end
% for a=1:20
%     while data_fct(cnt,4)<times(a)
%         active_conns(data_fct(cnt,1)+1,data_fct(cnt,2)+1)=0;
%         cnt=cnt+1;
%         if cnt>Nrecorded_flows
%             break
%         end
%     end
%     rack_conns=zeros(108);
%     for s=1:108
%         for d=1:108
%             if sum(sum(active_conns((s-1)*6+1:s*6,(d-1)*6+1:d*6)))>=6
%                 rack_conns(s,d)=1;
%             end
%         end
%     end
%     traff_densities(a)=sum(rack_conns(:))/(108*(108-1));
% end
% plot(times,traff_densities,'-o','linewidth',2);

% ---

% maxutil=max(data_util(:,1))
% lastutil=data_util(end,1)

% figure;
% cdfplot(data_fct(:,4));
% grid on;
% box on;
% ax=gca;
% ax.FontSize=12;
% title('');
% xlabel('FCT');
% ylabel('CDF');

data=csvread('../../../Figure1_traffic/websearch.csv');
flowsize=data(:,1).';
flowcdf=data(:,2).';

Nflowsize=length(flowsize);

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
meanFCTs=zeros(1,Nflowsize);
FCTs99=zeros(1,Nflowsize);
for fs=1:Nflowsize
    filtered_flows{fs}=zeros(Nfiltered_flows(fs),5);
end
figure;
hold on;
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
        meanFCTs(fs)=mean(filtered_flows{fs}(:,4));
        cdfplot(filtered_flows{fs}(:,4));
        
        perc=.99; % percentile
        temp=sort(filtered_flows{fs}(:,4));
        [n,~]=size(temp);
        FCTs99(fs)=temp(round(perc*n));
        
    end
end
grid on;
box on;
ax=gca;
ax.FontSize=12;
title('');
xlabel('FCT (ms)');
ylabel('CDF');

figure;
hold on;
plot(flowsize,1e3*meanFCTs,'o','linewidth',2);
plot(flowsize,1e3*FCTs99,'o','linewidth',2);
plot(flowsize,1e3*1e3*(flowsize+64)*8/10e9,'--k','linewidth',1);
grid on;
box on;
ax=gca;
ax.XScale='log';
ax.YScale='log';
ax.FontSize=16;
xlabel('Flow size (bytes)');
ylabel('Mean FCT (\mus)');
% plot(17*[1e6 1e6],ax.YLim,'--r');
xlim([4000 30e6]);
ax.XTick=[10^4 10^5 10^6 10^7];
ylim([10^1 10^5]);


figure;
hold on;
plot(flowsize,flowcdf);
plot(flowsize,cumsum(Nfiltered_flows)/Nrecorded_flows*perc_load*100);
ax=gca;
ax.XScale='log';

% ---

N100kB_flows=0;
for i=1:Nrecorded_flows
    if data_fct(i,3)==100e3
        N100kB_flows=N100kB_flows+1;
    end
end
filtered_100kB_flows=zeros(N100kB_flows,5);
for i=1:length(data_fct(:,1))
    if data_fct(i,3)==100e3
        filtered_100kB_flows(cnt,:)=data_fct(i,:);
        cnt=cnt+1;
    end
end
mean_100kB_FCT=mean(filtered_100kB_flows(:,4))
perc=.99; % percentile
temp=sort(filtered_100kB_flows(:,4));
[n,~]=size(temp);
FCT_100kB_99=temp(round(perc*n))

figure;
hold on;
h=cdfplot(filtered_100kB_flows(:,4));
h.LineWidth=2;
plot(FCT_100kB_99,perc,'o','linewidth',2);
grid on;
box on;
ax=gca;
ax.FontSize=16;
title('');
xlabel('FCT (ms)');
ylabel('CDF');
xlim([0 400]);

% save(matname,'data_fct','data_util','filtered_100kB_flows');

%%











