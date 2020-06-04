
% topology parameters:

k=12; % packet switch radix
u=7; % number of uplinks
N=130; % number of racks
H=N*(k-u); % number of hosts

% -------

Nruns=1;
mean_hops=zeros(length(N),Nruns);

for b=1:Nruns
    
    fprintf(sprintf('Iteration %d\n',b));
    
    rama=2*sqrt(u-1); % get ramanujan bound
    A=const_rama(N,u,rama); % get graph adjacency
    
    % figure; % plot adjacency
    % spy(A);
    
    src=(1:N); % sweep all sources and destinations
    dst=(1:N);
    
    [costs,~]=dijkstra_vector(A,A,src,dst); % get number of "hops"
    cnt=0;
    costvect=zeros(1,N^2-N);
    for c=1:N
        for d=1:N
            if c~=d
                cnt=cnt+1;
                costvect(cnt)=costs(c,d); % number of CORE hops
                % (a ToR hop is when a packet goes through a ToR)
            end
        end
    end
    
    mean_hops(b) = mean(costvect);
end

figure;
spy(A);

rama_d=2*sqrt(u-1)
temp=sort(abs(eig(A)));
lambda=temp(end-1) % second eigenvalue

kpaths=8; % number of k-shortest paths to check

Ap=1./A; % condition A for finding k-shortest paths
Ap(Ap<1)=1;

costmat=zeros(N);
costvect=zeros(1,N^2-N);
pathsmat=cell(N);

cntcons=0;
for src=1:N  % sweep source (rack)
    for dst=1:N % sweep destination (rack)
        if src~=dst % not sending to self
            
            cntcons=cntcons+1; % increment the connection
            fprintf(sprintf('Connection %d of %d\n',cntcons,N^2-N));
            
            [paths,costs]=kShortestPath(Ap,src,dst,kpaths);
%             costs=costs+1; % convert into number of ToR hops
            Necmp=sum(costs==costs(1)); % number of equal-cost paths
            pathsmat{src,dst}=paths(1:Necmp);
            costmat(src,dst)=costs(1);
            costvect(cntcons)=costs(1);
        else
            costmat(src,dst)=nan;
        end
    end
end

%%

% save(sprintf('k_shortest_paths_n%d.mat',N),'pathsmat');

figure;
hold on;
h=cdfplot(costvect);
h.LineWidth=2;
ax=gca;
ax.FontSize=16;
title('');
ylabel('CDF');
xlabel('Number of hops');
xlim([0 12]);
grid on;
box on;

mean(costvect)

%% write out the topology file for the simulator:

writetopology(H,(k-u),u,N,A,pathsmat);
















