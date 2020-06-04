function []=defineTopology_simple_kshortestpaths(N,k,G)

% first line: <#servers> <#ToRdownlinks> <#ToRuplinks> <#ToRs>
% second line: <#slices> <time_in_slice_type_0> <time_in_slice_type_1> ...
% next #slices lines: tors that each uplink is connected to (-1 means disconnected)
%   (***tors indexed from zero)
% rest of lines: paths between ToRs
%   Format: <slice number>
%           <srcToR> <dstToR> <intermediate_queue0> <intermediate_queue1> ...
%           <srcToR> <dstToR> <intermediate_queue0> <intermediate_queue1> ...
%           ...
%           <next slice number>
%           ...
%   (***slices, ToRs, queues indexed from zero)
%   (***queue indexing starts from the first downlink (0) to the last
%   downlink (#downlinks-1) to the first uplink (#downlinks) to the last
%   uplink (#downlinks+#uplinks-1). The queues we record here will always
%   correspond to uplinks, so their indexing will start at (#downlinks))

%%

load(sprintf('topo_params_kshortest_N=%d_k=%d_G=%d.mat',N,k,G));

d=k/2; % # downlinks
u=k/2; % # uplinks

linkdelay=0.5; % microseconds (0.5 us = 100 meters)
linkrate=10e9; % bits/s
pktsize=1500; % bytes / packet
pktser=pktsize*8/linkrate; % seconds
queuepkts=8; % queue length
hop_delay_worst=linkdelay*1e6 + queuepkts*pktser*1e12; % picoseconds

% old way:
% eps0=(minmaxhops-1)*hop_delay_worst;
% del=hop_delay_worst;
% new way:
eps0=(minmaxhops-1)*hop_delay_worst + (queuepkts-1)*pktser*1e12;
del=linkdelay*1e6 + pktser*1e12;
r=10*1e6;

slicetypes=3; % (eps, delta, r) [in picoseconds!]

filename=sprintf('dynexp_kshortest_N=%d_k=%d_G=%d.txt',N,k,G);
fID=fopen(filename,'w');

fprintf(fID,'%d %d %d %d\n',N*d,d,u,N); % hosts, downlinks, uplinks, racks
fprintf(fID,'%d %d %d %d',slicetypes*N,ceil(eps0),ceil(del),ceil(r)); % # slices. (eps, delta, r) * Ntors

%%

for superslice=1:N
    
    % epsilon slice (all rotors up)
    % in "fast" rotor cycle, this is the "extra" guard time.
    % in "slow" rotor cycle, this is the same as delta (below).
    fprintf(fID,'\n');
    for srctor=1:N
        for ul=1:u
            vtemp=v(ul,superslice);
            if isnan(vtemp)==1
                if superslice==1
                    vtemp=v(ul,N); % wrap around
                else
                    vtemp=v(ul,superslice-1);
                end
            end
            dsttor=find(m_sol{ul,vtemp}(srctor,:)==1,1);
            if srctor*ul<N*u
                fprintf(fID,'%d ',dsttor-1); % indexed from zero
            else
                fprintf(fID,'%d',dsttor-1);
            end
        end
    end
    
    % delta slice
    % in the "fast" rotor cycle, this is the queuing + flight time
    % in the "slow" rotor cycle, same as epsilon (above)
    fprintf(fID,'\n');
    for srctor=1:N
        for ul=1:u
            vtemp=v(ul,superslice);
            if isnan(vtemp)==1
                if superslice==1
                    vtemp=v(ul,N); % wrap around
                else
                    vtemp=v(ul,superslice-1);
                end
            end
            dsttor=find(m_sol{ul,vtemp}(srctor,:)==1,1);
            if srctor*ul<N*u
                fprintf(fID,'%d ',dsttor-1);
            else
                fprintf(fID,'%d',dsttor-1);
            end
        end
    end
    
    % r slice (reconfiguration)
    fprintf(fID,'\n');
    for srctor=1:N
        for ul=1:u
            vtemp=v(ul,superslice);
            if isnan(vtemp)==1
                if srctor*ul<N*u
                    fprintf(fID,'%d ',-1);
                else
                    fprintf(fID,'%d',-1);
                end
            else
                dsttor=find(m_sol{ul,vtemp}(srctor,:)==1,1);
                if srctor*ul<N*u
                    fprintf(fID,'%d ',dsttor-1);
                else
                    fprintf(fID,'%d',dsttor-1);
                end
            end
        end
    end
end


for sliceind=1:N
    
    % ----- print the paths for slices 0, 3, 6, ..., then:
    % ----- print the paths for slices 1, 4, 7, ..., then:
    % ----- print the paths for slices 2, 5, 8, ...
    
    for os=0:2 % offset
        
        fprintf(fID,'\n%d',slicetypes*sliceind-slicetypes+os);
        
        vtemp=v(:,sliceind);
        
        for srctor=1:N
            for dsttor=1:N
                if srctor~=dsttor
                    [~,npaths]=size(paths{sliceind}{srctor,dsttor});
                    for pathind=1:npaths
                        fprintf(fID,'\n%d %d ',srctor-1,dsttor-1); % indexed from zero
                        % get and print the ToR queues for each hop:
                        [~,nhop]=size(paths{sliceind}{srctor,dsttor}{pathind});
                        for hop=1:nhop-1
                            
                            s_src=paths{sliceind}{srctor,dsttor}{pathind}(hop); % switch source port (rack)
                            s_dst=paths{sliceind}{srctor,dsttor}{pathind}(hop+1); % switch output port (rack)
                            foundswitch=0;
                            qind=0;
                            while foundswitch==0 % sweep over the rotor switches
                                qind=qind+1;
                                if isnan(vtemp(qind))==0 % this queue (rotor) is active
                                    if m_sol{qind,vtemp(qind)}(s_src,s_dst)==1 % this is the rotor switch
                                        foundswitch=1; % exit loop
                                    end
                                end
                            end
                            
                            if hop~=nhop-1
                                fprintf(fID,'%d ',d + qind-1); % indexed from zero
                            else
                                fprintf(fID,'%d',d + qind-1);
                            end
                        end
                    end
                end
            end
        end
    end
end

fclose(fID);





















