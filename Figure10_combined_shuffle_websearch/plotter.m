
% hand-entered the throughputs processed using:
% ./opera/postprocess/process_FCT_and_UTIL.m
l_opera=[.0001 .01 .025 .05 .07 .08 .09 .1];
tp_opera=[.97 .93 .87 .76 .7 .66 .62 .5];

l_u7=[.0001 .01 .025 .05 .1 .2 .24];
tp_u7=.24*ones(size(l_u7)); % based on saturation point

l_31ft=[.0001 .01 .025 .05 .1 .2 .23];
tp_31ft=.23*ones(size(l_31ft)); % based on saturation point

MS=6;

figure;
hold on;
h1=plot(l_opera,tp_opera,'-o','linewidth',2,'markersize',MS);
% plot(-1,-1);
h2=plot(l_u7,tp_u7,'-v','linewidth',2,'markersize',MS);
h3=plot(l_31ft,tp_31ft,'-*','linewidth',2,'markersize',MS+4);

plot(l_opera(end)*[1 1],[tp_opera(end) 0],'--','color',h1.Color,'linewidth',2);
plot(l_u7(end)*[1 1],[tp_u7(end) 0],'--','color',h2.Color,'linewidth',2);
plot(l_31ft(end)*[1 1],[tp_31ft(end) 0],'--','color',h3.Color,'linewidth',2);

xlim([.007 .5]);
ylim([0 1]);

grid on;
box on;

ax=gca;
ax.FontSize=16;

ax.XScale='log';
ax.XTick=[.01 .025 .05 .1 .2 .4];
ax.XTickLabel={'1%','2.5%','5%','10%','20%','40%'};
ax.XMinorGrid='off';
ax.XMinorTick='off';

uistack(h2,'top');
uistack(h1,'top');

% ax.XTick=(0:.01:.6);

xlabel('Websearch load');
ylabel('Throuhgput');

hleg=legend([h1 h2 h3], ...
    'Opera','{\itu} = 7 exp.','3:1 F.C.');
hleg.Location='northeast';
hleg.FontSize=11;

set(gcf,'position',[543 506 560  335]);


an=annotation('doublearrow');
an.LineWidth=2;
an.X=[.62 .76];
an.Y=[.24 .24];
ht=text(.135,.14,'2x');
ht.FontSize=16;

an=annotation('doublearrow');
an.LineWidth=2;
an.X=[.17 .17];
an.Y=[.38 .865];
ht=text(.01,.6,'4x');
ht.FontSize=16;



ax.XColor=[0 0 0];
ax.YColor=[0 0 0];
ax.GridColor=[0 0 0];
ax.MinorGridColor=[0 0 0];
ax.GridAlpha=.1;
fig = gcf;
fig.PaperPositionMode = 'auto';
fig_pos = fig.PaperPosition;
fig.PaperSize = [fig_pos(3) fig_pos(4)];
print(fig,'tp_v_load','-dpdf')



