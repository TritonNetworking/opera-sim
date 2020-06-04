
% ----- define traffic pattern between HOSTS:

CASE=2;

if CASE==1
    u=6;
    Nrack=108;
    Hosts_p_rack=6;
elseif CASE==2
    u=7;
    Nrack=130;
    Hosts_p_rack=5;
elseif CASE==3
    u=8;
    Nrack=162;
    Hosts_p_rack=4;
end

H=Nrack*Hosts_p_rack; % number of hosts

filename=sprintf('flows_100kB_%dhosts_u%d',H,u);

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

nflows=Ncons;
% % stagger the start times over a 1 ms interval
% flowmat=zeros(nflows,1);
% for a=1:nflows
%     flowmat(a)=rand*1000000; % nanoseconds
% end
% stagger the start times over a 10 ms interval
flowmat=zeros(nflows,1);
for a=1:nflows
    flowmat(a)=rand*10000000; % nanoseconds
end
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

write_to_htsim_file(flowmat,filename);














