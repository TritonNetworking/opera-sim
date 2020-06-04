
%% inputs:

N=108; % number of racks total
k=12; % packet switch ToR radix, N must be divisible by k/2

Nruns=1; % number of random topology realizations to try

topotype=1;
% topotype == 1 -> 1st shortest path between each ToR in each slice
% topotype == 2 -> k-shortest Opera paths

% don't change this:
G=1; % set number of rotor groups

%% generate topology

% ---------------------------------
% get the matchings

if mod(N,(k/2))~=0
    fprintf('\n\nERROR: `N` not divisible by `k/2`, choose different parameters\n\n');
    error('`N` not divisible by `k/2`, choose different parameters');
end

fprintf('\nDetermining matchings...\n');
Nbase=N;
lifted=0;
if Nbase > 64
    % we need to use lifting to construct the graph
    % currently we only support even Nbase in the lifting code...
    lifted=1;
    base=fliplr(8:2:64);
    done=0;
    cnt=0;
    while done==0
        cnt=cnt+1;
        
        if mod(N/base(cnt),2)==0
            done=1;
            Nbase=base(cnt);
        end
        if cnt==length(base)
            fprintf('\n\nERROR: no lifting solution, try a different `N`\n\n');
            error('no lifting solution, try a different `N`');
        end
    end
end

rng('shuffle'); % shuffle random number generator
Niter=10000;
strtmat=triu(ones(Nbase));
lvl=0;
Asol=cell(1,Nbase);
itercnt=0;
pflag=1;
if mod(Nbase,2)~=0
    % for an odd number of endpoints:
    [P,~,~]=decomp_recursive_odd(Nbase,Niter,strtmat,lvl,Asol,itercnt,pflag);
else
    [P,~,~]=decomp_recursive_original(Nbase,Niter,strtmat,lvl,Asol,itercnt,pflag);
end

fprintf('\nFound matchings.\n');

save(sprintf('Decomp_N=%d.mat',Nbase),'P');

% ---------------------------------
% lift the graph if necessary:
if lifted==1
    fprintf('\n\nLifting Graph...\n');
    lift_construction(Nbase,N);
    fprintf('\nLifted.\n');
end

% ---------------------------------
% load the matchings in different ways and find the best one & output `m_sol`
pflag=0; % print flag
fprintf('\n\nSearching for best topology...\n');
pick_topo(N,Nbase,k,lifted,pflag,Nruns,G);
fprintf('\nPicked the best topology.\n');

if topotype==1
    
    % ---------------------------------
    % get the shortest paths & output `topo_params.mat`
    fprintf('\n\nGetting shortest paths...\n');
    path_calc_shortest(N,k,G)
    
    % ---------------------------------
    % define the topology for htsim
    fprintf('\n\nWriting htsim topology file...\n');
    defineTopology_simple_1path(N,k,G);
    
elseif topotype==2
    
    % ---------------------------------
    % get the shortest paths & output `topo_params.mat`
    fprintf('\n\nGetting shortest paths...\n');
    path_calc_kshortest(N,k,G)
    
    % ---------------------------------
    % define the topology for htsim
    fprintf('\n\nWriting htsim topology file...\n');
    defineTopology_simple_kshortestpaths(N,k,G);
    
end

fprintf('\n\nDONE\n\n');






















