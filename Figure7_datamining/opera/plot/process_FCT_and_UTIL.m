% process data:

clear;clc;

perc=1;

fname=sprintf('../sim/FCT_pfab_cwnd20_%dperc.txt',perc);

matname=sprintf('FCT_%dperc.mat',perc);

savemat=1; % flag for saving .mat file

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

% times=(10:10:990);
% traff_densities=zeros(1,length(times));
% cnt=1;
% active_conns=ones(648);
% for a=1:108
%     active_conns((a-1)*6+1:a*6,(a-1)*6+1:a*6)=zeros(6);
% end
% for a=1:length(times)
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
% ax.FontSize=16;
% title('');
% xlabel('FCT');
% ylabel('CDF');

data=csvread('../../../Figure1_traffic/datamining.csv');
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
        
        perc=.9; % percentile
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
plot(17*[1e6 1e6],ax.YLim,'--r');
xlim([100 1e9]);
ax.XTick=[10^2 10^4 10^6 10^8];
ylim([1 10^6]);


figure;
hold on;
plot(flowsize,flowcdf);
plot(flowsize,cumsum(Nfiltered_flows)/10195);
ax=gca;
ax.FontSize=12;
ax.XScale='log';
ylabel('CDF');
xlabel('Flow size (bytes)');

if savemat==1
    save(matname,'data_fct');
end

%     intervals=[0 10 20 30 40 50 60 70 80 90 100];
%     avgfcts=zeros(1,10);
%     for a=1:10
%         check_inds=data_fct(:,5)>=intervals(a);
%         data_filt=data_fct(check_inds,:);
%         check_inds=data_filt(:,5)<intervals(a+1);
%         data_filt=data_filt(check_inds,:);
%         avgfcts(a)=mean(data_filt(:,4));
%     end;  
%     figure;
%     plot(avgfcts);
%     ylim([0 10]);
    
%%
    








