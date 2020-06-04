% process data:

fname=sprintf('output_100kB_3to1_pull1_cwnd40_q60.txt');

fid=fopen(fname,'r');
string=fgetl(fid);
cnt_line=0;
cnt_fct=0;
cnt_util=0;
while ischar(string)
    if strcmp(string(1:3),'FCT')
        cnt_line=cnt_line+1;
        cnt_fct=cnt_fct+1;
        data_fct_3to1(cnt_fct,:)=sscanf(string,'%*s %d %d %d %f %f %*\n').';
        % src, dst, bytes, fct, time_started
        string=fgetl(fid);
    elseif strcmp(string(1:4),'Util')
        cnt_line=cnt_line+1;
        cnt_util=cnt_util+1;
        data_util_3to1(cnt_util,:)=sscanf(string,'%*s %f %f %*\n').';
        string=fgetl(fid);
    else
        cnt_line=cnt_line+1;
        %             fprintf(sprintf('Load = %.2f, data format mismatch\n',Load1(lind)));
        %             fprintf(sprintf('   >> line = %d\n',cnt_line));
        string=fgetl(fid);
    end
end
fclose(fid);

Nrecorded_flows=length(data_fct_3to1(:,1));

% ---

figure;
hold on;
plot(data_util_3to1(:,2),data_util_3to1(:,1),'-o','linewidth',2);
ylim([0 1]);
ax=gca;
ax.FontSize=16;
xlabel('Time (ms)');
ylabel('Throughput');
grid on;
box on;
% xlim([data_util(1,2) data_util(end,2)]);
xlim([0 300]);

% times=(1:1:100);
% Ntimes=length(times);
% traff_densities=zeros(1,Ntimes);
% cnt=1;
% active_conns=ones(648);
% for a=1:108
%     active_conns((a-1)*6+1:a*6,(a-1)*6+1:a*6)=zeros(6);
% end
% for a=1:Ntimes
%     if cnt<Nrecorded_flows
%         while data_fct(cnt,4)<times(a)
%             active_conns(data_fct(cnt,1)+1,data_fct(cnt,2)+1)=0;
%             cnt=cnt+1;
%             if cnt>Nrecorded_flows
%                 break
%             end
%         end
%     else
%         active_conns=zeros(648);
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
% plot(times,traff_densities,'-','linewidth',2);

% ---

% maxutil=max(data_util(:,1))
% lastutil=data_util(end,1)

figure;
h=cdfplot(data_fct_3to1(:,4));
h.LineWidth=2;
grid on;
box on;
ax=gca;
ax.FontSize=16;
title('');
xlabel('FCT (ms)');
ylabel('CDF');
xlim([0 300]);



save('3to1ft_100kB_only.mat','data_fct_3to1','data_util_3to1');
    
%%
    











