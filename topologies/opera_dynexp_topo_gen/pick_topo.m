function []=pick_topo(N,Nbase,k,lifted,pflag,Nruns,G)

%%

u=k/2; % endpoint fanout, also == number of rotors

if lifted==1
    load(sprintf('lifted_Decomp_Nbase=%d_N=%d.mat',Nbase,N),'P');
else
    load(sprintf('Decomp_N=%d.mat',N),'P');
end

mpr=N/u; % matchings / rotor
min_core_hops_r=cell(Nruns,N);

m=cell(Nruns,1);

% workers=3;
% p=parpool(workers);
% parfor iter=1:Nruns
for iter=1:Nruns
    
    fprintf(sprintf('Iteration %d of %d\n',iter,Nruns));
    
    P_rand=P(randperm(N)); % randomize order
    
    % assign matchings to each rotor switch
    m{iter}=cell(u,mpr);
    cnt=0;
    for a=1:u
        for b=1:mpr
            cnt=cnt+1;
            m{iter}{a,b}=P_rand{cnt};
        end
    end
    
    % u rotors, divided into G groups
    v=[];
    for a=1:mpr
        v=[v [a*ones(1,u/G-1) nan]];
    end
    for a=1:u/G-1
        v=[v;circshift(v(1,:),[0 a])];
    end
    if G>1 % if there are multiple rotor groups
        v0=v;
        for g=2:G
            v=[v;v0];
        end
    end
    v=[v v];
    
    % During reconfigs:
    for slc=1:N/G
        fprintf(sprintf('Slice %d of %d\n',slc,N/G));
        A=zeros(N);
        for b=1:u
            if isnan(v(b,slc))==0
                A=A+m{iter}{b,v(b,slc)};
            end
        end
        
        src=(1:N);
        dst=(1:N);
        
        [opthops,~]=dijkstra_vector(A,A,src,dst);
        
        cnt=0;
        for s=1:N
            for d=1:N
                if s~=d
                    cnt=cnt+1;
                    min_core_hops_r{iter,slc}(cnt)=opthops(s,d);
                end
            end
        end
    end
    
end

% delete(p);

save(sprintf('core_hops_random_N=%d_k=%d_G=%d.mat',N,k,G),'-v7.3');

%% plot the hop cdfs of the topologies:

% load(sprintf('ToR_hops_random_N=%d_k=%d.mat',N,k)); % for standard case

minhops_all=cell(Nruns,1);
for iter=1:Nruns
    minhops_all{iter}=[];
    for slc=1:N/G
        minhops_all{iter}=[minhops_all{iter} min_core_hops_r{iter,slc}];
    end
end

% path lengths:

maxhops=inf*ones(1,Nruns);
meanhops=inf*ones(1,Nruns);
for iter=1:Nruns
    maxhops(iter)=max(minhops_all{iter}); % store the worst-case # hops
    meanhops(iter)=mean(minhops_all{iter}); % store the mean # hops
end

if pflag==1
    
    figure;
    hold on;
    for iter=1:Nruns
        h=cdfplot(minhops_all{iter});
        %     h.LineWidth=2;
        tempx=h.XData;
        tempy=h.YData;
        inds=tempy==1;
        if inds(end-1)==1
            plot(tempx(end-1),tempy(end-1),'o','color',h.Color);
%             maxhops(iter)=tempx(end-1); % store the worst-case # hops
%             meanhops(iter)=mean(minhops_all{iter}); % store the mean # hops
        end
    end
    grid on;
    box on;
    title('');
    xlim([0 12]);
    ylim([0 1]);
    ax=gca;
    ax.FontSize=16;
    xlabel('Number ToR-to-ToR hops');
    ylabel('CDF');
    
end

%% choose the best topology:

minmaxhops=min(maxhops);
inds_minmax=find(maxhops==minmaxhops);
[~,inds2]=min(meanhops(inds_minmax));
best_ind=inds_minmax(inds2);

fprintf(sprintf('\nMaximum_hops = %d\nMean hops = %0.2f\n',minmaxhops,meanhops(best_ind)));

if isinf(minmaxhops)==1
    fprintf('\n\nERROR: Topology search failed: disconnected network\n\n');
    error('Topology search failed: disconnected network');
end


%% plot the hop cdf of the chosen topology:

iter=best_ind;
minhops_all_save=minhops_all{iter};
save(sprintf('minhops_all_N=%d_k=%d_G=%d.mat',N,k,G),'minhops_all_save');

figure;
hold on;
h=cdfplot(minhops_all_save);
tempx=h.XData;
tempy=h.YData;
inds=tempy==1;
if inds(end-1)==1
    plot(tempx(end-1),tempy(end-1),'o','color',h.Color);
end
grid on;
box on;
title('');
xlim([0 12]);
ylim([0 1]);
ax=gca;
ax.FontSize=16;
xlabel('Number ToR hops');
ylabel('CDF');



%% save the best one:

m_sol=m{best_ind};

dummyvar=1;
save(sprintf('msol_N=%d_k=%d_G=%d.mat',N,k,G),'dummyvar','m_sol','minmaxhops','-v7.3');













