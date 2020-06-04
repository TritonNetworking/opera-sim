

%% throughput:

load('opera/Opera_100kB_only_5paths_1hop.mat')
load('u7_expander/u7exp_100kB_only.mat')
load('3to1_clos/3to1ft_100kB_only.mat')

figure;
hold on;
% h0=plot([(1:1:100).' ; 100+data_util_opera(:,2) ; (201:1:1000).'], ...
%     [zeros(1,100).' ; data_util_opera(:,1) ; zeros(1,800).'],'-','linewidth',2);
% hexp=plot([(1:1:100).' ; 100+data_util_u7exp(:,2) ; (401:1:1000).'], ...
%     [zeros(1,100).' ; data_util_u7exp(:,1) ; zeros(1,600).'],'-.','linewidth',2);
% h31=plot([(1:1:100).' ; 100+data_util_3to1(:,2) ; (401:1:1000).'], ...
%     [zeros(1,100).' ; data_util_3to1(:,1) ; zeros(1,600).'],'--','linewidth',2);

h0=plot([(1:1:100).' ; 100+data_util_opera(:,2) ; (201:1:1000).'], ...
    [zeros(1,100).' ; data_util_opera(:,1) ; zeros(1,800).'],'-','linewidth',2);
hexp=plot([(1:1:100).' ; 100+data_util_u7exp(:,2) ; (401:1:1000).'], ...
    [zeros(1,100).' ; data_util_u7exp(:,1) ; zeros(1,600).'],'-','linewidth',2);
h31=plot([(1:1:100).' ; 100+data_util_3to1(:,2) ; (401:1:1000).'], ...
    [zeros(1,100).' ; data_util_3to1(:,1) ; zeros(1,600).'],'-','linewidth',2);

ylim([0 1]);
ax=gca;
ax.FontSize=16;
xlabel('Time (ms)');
ylabel('Throughput');
grid on;
box on;
xlim([90 350]);
ax.XTick=(100:50:350);
ax.XTickLabel={'0','50','100','150','200','250','300','350'};
ax.YTick=[0 .25 .5 .75 1];

% hleg=legend([h0 hexp h31],'Opera','{\itu} = 7 exp.','3:1 F.C.');
% hleg=legend([h0 hexp h31],'LEED','Expander graph','3:1 Fat Tree');
hleg=legend([h0 hexp h31],'Time-varying {\itu} = 6 expander', ...
    'Static {\itu} = 7 expander','3:1 folded Clos');
hleg.FontSize=12;
% hleg.Location='northeast';

set(gcf,'position',[543 506 560 264]);

ax.XColor=[0 0 0];
ax.YColor=[0 0 0];
ax.GridColor=[0 0 0];
ax.MinorGridColor=[0 0 0];
ax.GridAlpha=.1;
fig = gcf;
fig.PaperPositionMode = 'auto';
fig_pos = fig.PaperPosition;
fig.PaperSize = [fig_pos(3) fig_pos(4)];
% print(fig,'tp','-dpdf')
print(fig,'tp_fig_nsf','-dpdf')



