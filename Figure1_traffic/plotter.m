
%%

% read traffic files:

% pfabric websearch workload:
data=csvread('websearch.csv');
flowsize=data(:,1).';
cdf=data(:,2).';
bytes=[0 flowsize(2:end).*diff(cdf)];
bytes=bytes/sum(bytes);
bytes=cumsum(bytes);

% FB Hadoop - interrack:
data=csvread('FB_Hadoop_Inter_Rack_FlowCDF.csv');
flowsize1=data(:,1).';
cdf1=data(:,2).';
bytes1=[0 flowsize1(2:end).*diff(cdf1)];
bytes1=bytes1/sum(bytes1);
bytes1=cumsum(bytes1);
% avg_flowsize1=sum(flowsize1(2:end).*diff(cdf1)) % bytes / flow

% pfabric datamining workload:
data=csvread('datamining.csv');
flowsize2=data(:,1).';
cdf2=data(:,2).';
bytes2=[0 flowsize2(2:end).*diff(cdf2)];
bytes2=bytes2/sum(bytes2);
bytes2=cumsum(bytes2);
% avg_flowsize2=sum(flowsize2(2:end).*diff(cdf2)) % bytes / flow

%% cdf 1:

figure;
hold on;
h1=plot(flowsize,cdf,'-.','linewidth',2);
h2=plot(flowsize1,cdf1,'--','linewidth',2);
plot(0,0);
plot(0,0);
h3=plot(flowsize2,cdf2,'-','linewidth',2);

ax=gca;
ax.FontSize=14;
ax.XScale='log';
grid on;
box on;
xlim([10^2 10^9]);
hleg=legend([h3 h1 h2],'Datamining [21]','Websearch [4]','Hadoop [39]');
hleg.Location='southeast';
hleg.FontSize=12;
xlabel('Flow size (bytes)');
ylabel('CDF of flows');
ax.MinorGridLineStyle='none';
ax.XTick=[10^2 10^3 10^4 10^5 10^6 10^7 10^8 10^9];

fig=gcf;
fig.Position=[543.2857 506.1429 560.000 230.2857];

ax.XColor=[0 0 0];
ax.YColor=[0 0 0];
ax.GridColor=[0 0 0];
ax.MinorGridColor=[0 0 0];
ax.GridAlpha=.1;

fig = gcf;
fig.PaperPositionMode = 'auto';
fig_pos = fig.PaperPosition;
fig.PaperSize = [fig_pos(3) fig_pos(4)];

% print(fig,'traff_1','-dpdf')

%% cdf 2

figure;
hold on;
h1=plot(flowsize,bytes,'-.','linewidth',2);
h2=plot(flowsize1,bytes1,'--','linewidth',2);
plot(0,0);
plot(0,0);
h3=plot(flowsize2,bytes2,'-','linewidth',2);

ax=gca;
ax.FontSize=14;
ax.XScale='log';
grid on;
box on;
xlim([10^2 10^9]);
hleg=legend([h3 h1 h2],'Datamining [21]','Websearch [4]','Hadoop [39]');
hleg.Location='northwest';
hleg.FontSize=12;
xlabel('Flow size (bytes)');
ylabel('CDF of bytes');
ax.MinorGridLineStyle='none';
ax.XTick=[10^2 10^3 10^4 10^5 10^6 10^7 10^8 10^9];

fig=gcf;
fig.Position=[543.2857 506.1429 560.000 230.2857];

ax.XColor=[0 0 0];
ax.YColor=[0 0 0];
ax.GridColor=[0 0 0];
ax.MinorGridColor=[0 0 0];
ax.GridAlpha=.1;

fig.PaperPositionMode = 'auto';
fig_pos = fig.PaperPosition;
fig.PaperSize = [fig_pos(3) fig_pos(4)];

% print(fig,'traff_2','-dpdf')






















