
% ----- define traffic pattern between HOSTS:

Nrack=108;
Hosts_p_rack=6;

H=Nrack*Hosts_p_rack; % number of hosts

loadfrac0=.01; % fraction of theoretically possible load

% ----- define simulation length (time):

totaltime=10.001; % seconds, probably want a little extra here...

filename=sprintf('flows_%.0fpercLoad_10sec_%dhosts',100*loadfrac0,H);

%% prio traffic:

% NOTE: only inter-rack traffic allowed

% --- uniform
Nactivehosts=H;
Ncons=H*(H-Hosts_p_rack); % number of possible connections
srcdst=zeros(Ncons,2);
cnt=0;
for a=1:H % sources
    for b=1:H % destinations
        if ceil(a/Hosts_p_rack)~=ceil(b/Hosts_p_rack)
            % hosts are not in the same rack
            cnt=cnt+1;
            srcdst(cnt,:)=[a b];
        end
    end
end

tmindices=(0:1:Ncons); % these indices index `srcdst`
tmcdf=(0:1/Ncons:1);

% ----- define flow size distribution (bytes):

% pfabric datamining workload:
data=csvread('../../../Figure1_traffic/datamining.csv');
flowsize=data(:,1).';
flowcdf=data(:,2).';

% ----- define load:

avg_flowsize=sum(flowsize(2:end).*diff(flowcdf)); % bytes / flow
linkrate=10e9/8; % bytes / second

lambda_host_max=linkrate/avg_flowsize; % flows / second (per host)
lambda_host=loadfrac0*lambda_host_max; % flows / second for each host

lambda_network=Nactivehosts*lambda_host; % flows / second for entire network


% ----- compute the flow data:

% estimate how many flows there will be:
nflows_est=ceil(lambda_network*totaltime)

flowmat1=zeros(nflows_est,1);

% ----- 1 pick all times (and get number of flows)

fprintf('Getting PRIO flow start times...\n');
tic;

crt_time=0;
cnt=0;
while crt_time<totaltime
    next_time=-log(1-rand)/lambda_network;
    crt_time=crt_time+next_time;
    cnt=cnt+1;
    flowmat1(cnt)=crt_time;
end
ind=find(flowmat1>totaltime,1);
flowmat1(ind:end)=[];

[nflows,~]=size(flowmat1);
flowmat1=round(1e9*repmat(flowmat1,1,4)); % src, dst, size (bytes), start time (nanoseconds)

t=toc;
fprintf(sprintf('    Finished in %.2f seconds\n',t));

% ----- get flow sizes (bytes)

fprintf('Getting PRIO flow sizes...\n');
tic;

randvect=rand(1,nflows);

for a=1:nflows
%     [~,ind]=min(abs(flowcdf-randvect(a))); % this was skewing the effective rate too low...
    ind=find(flowcdf-randvect(a)>=0,1);
    flowmat1(a,3)=flowsize(ind);
end

t=toc;
fprintf(sprintf('    Finished in %.2f seconds\n',t));

% ----- get sources and destinations

fprintf('Getting PRIO flow sources & destinations...\n');
tic;

randvect=rand(1,nflows);

for a=1:nflows
    %     index=ceil(interp1(tmcdf,tmindices,randvect(a)));
    % faster to not interpolate...
    ind=find(randvect(a)-tmcdf<=0,1);
    index=tmindices(ind);
    flowmat1(a,1)=srcdst(index,1);
    flowmat1(a,2)=srcdst(index,2);
end

t=toc;
fprintf(sprintf('    Finished in %.2f seconds\n',t));

fprintf(sprintf('\n\nSpecified fraction of capacity = %1.3f\n',loadfrac0));
fprintf(sprintf('Actual fraction of capacity = %1.3f\n\n',sum(flowmat1(:,3))/(totaltime*H*linkrate)));

%% ----- write to file:

write_to_htsim_file(flowmat1,filename);














