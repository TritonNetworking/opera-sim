
% ----- define traffic pattern between HOSTS:

Nrack=108;
Hosts_p_rack=6;

H=Nrack*Hosts_p_rack; % number of hosts

%% bulk traffic:

Ncons=H^2-Nrack*Hosts_p_rack^2; % number of possible connections
srcdst=zeros(Ncons,2);
cnt=0;
for a=1:H % sources
    srcrack=ceil(a/Hosts_p_rack);
    for b=1:H % destinations
        dstrack=ceil(b/Hosts_p_rack);
        if srcrack~=dstrack
            cnt=cnt+1;
            srcdst(cnt,:)=[a b];
        end
    end
end

% make the flows:
start_times=1; % nanoseconds

nflows=Ncons;

flowmat=start_times*ones(nflows,1);
flowmat=repmat(flowmat,1,4);

% flow sizes:
randvect=rand(1,nflows);
for a=1:nflows
    flowmat(a,3)=100000; % all 100 kB flows
end

% sources and destinations:
for a=1:nflows
    flowmat(a,1)=srcdst(a,1);
    flowmat(a,2)=srcdst(a,2);
end

%% ----- write to file:

filename=sprintf('flows_100kB_%dhosts',H);

write_to_htsim_file(flowmat,filename);














