function []=path_calc_kshortest(N,k,G)
%%

% NOTE:
% this version looks for k shortest paths that are no longer than the 
% shortest path for each connection.

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
Npaths=cell(1,N/G);
% `paths` contains the sequence of ToR indices along each shortest path
% paths{slice}{source_rack, destination_rack}{path}

k_paths_to_check=4;
max_paths_to_check=12;

parpool(8);

parfor slc=1:N/G % slice
% for slc=1:N/G % slice
    
    paths{slc}=cell(N,N);
    Npaths{slc}=zeros(1,N^2-N);
    cnt=0;
    
    A=zeros(N); % adjacency matrix for this slice
    for b=1:u
        if isnan(v(b,slc))==0
            A=A+m_sol{b,v(b,slc)};
        end
    end
    
    for s=1:N % source ToR
        fprintf(sprintf('Slice %d of %d, source %d of %d\n',slc,N/G,s,N));
        for d=1:N % dest ToR
            if s~=d
                scale=1;
                while scale>0
                    % get the shortest ToR to ToR paths:
                    [paths_temp,costs_temp]=kShortestPath(A,s,d,scale*k_paths_to_check);
                    if max(costs_temp)>min(costs_temp)
                        ind=find(costs_temp>costs_temp(1),1)-1;
                        paths{slc}{s,d}=paths_temp(1:ind);
                        
                        cnt=cnt+1;
                        Npaths{slc}(cnt)=ind;
                        scale=0;
                    elseif scale*k_paths_to_check >= max_paths_to_check
                        paths{slc}{s,d}=paths_temp;
                        
                        cnt=cnt+1;
                        Npaths{slc}(cnt)=max_paths_to_check;
                        scale=0;
                    else
                        scale=scale+1;
                    end
                end
            end
        end
    end
    
end

delete(gcp);

% temp=[];
% for a=1:N/G
%     temp=[temp Npaths{a}];
% end
% figure;
% h=cdfplot(temp);
% h.LineWidth=2;
% ax=gca;
% ax.FontSize=14;
% xlabel('Number of paths');
% ylabel('CDF');
% title('');


adummyvar=1;
save(sprintf('topo_params_kshortest_N=%d_k=%d_G=%d.mat',N,k,G),'-v7.3');

