function []=path_calc_shortest(N,k,G)
%%

% N=108; % number of racks
% k=12; % packet switch radix
u=k/2; % endpoint fanout, also == number of rotors

load(sprintf('msol_N=%d_k=%d_G=%d.mat',N,k,G));
% Note:
% `m_sol` contains the matchings implemented by each rotor switch
% m_sol{rotor_switch_index, matching_index}

mpr=N/u; % matchings / rotor

% Note:
% `v` indexes the rotor matchings present during each topology slice
% v(rotor_switch_index, slice)
% NaN corresponds to a switch reconfiguring
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

paths=cell(1,N/G);
% `paths` contains the sequence of ToR indices along each shortest path
% paths{slice}{source_rack, destination_rack}

% ports=cell(1,N);

% `ports` contains the sequence of ToR ports along each shortest path
% ports{slice}{source_rack, destination_rack}
% port 1 is connected to rotor switch 1, port u is connected to rotor switch u

for slc=1:N/G
    fprintf(sprintf('Iteration %d of %d\n',slc,N/G));
    A=zeros(N); % adjacency matrix for this slice
    for b=1:u
        if isnan(v(b,slc))==0
            A=A+m_sol{b,v(b,slc)};
        end
    end
    % get the shortest ToR to ToR paths:
    [~,paths{slc}]=dijkstra_vector(A,A,(1:N),(1:N));
end

adummyvar=1;
save(sprintf('topo_params_N=%d_k=%d_G=%d.mat',N,k,G),'-v7.3');

