function []=writetopology(H,dl,ul,N,A,pathsmat)

% this writes the topology file for htsim

filename=sprintf('expander_N=%d_u=%d_ecmp.txt',N,ul);

fID=fopen(filename,'w');

fprintf(fID,'%d %d %d %d\n',H,dl,ul,N);

for row=1:N
    tempstring=sprintf('%d ', A(row,:));
    fprintf(fID,'%s\n',tempstring(1:end-1));
end

% -1 is because we index from 0 in the simulator
writecnt=0;
for src=1:N
    for dst=1:N
        if src ~= dst
            writecnt=writecnt+1;
            % get number of paths:
            [~,npaths]=size(pathsmat{src,dst});
            for pathind=1:npaths
                
                tempstring=sprintf('%d ',[pathsmat{src,dst}{pathind}(1) pathsmat{src,dst}{pathind}(end)]-1);
                hops=length(pathsmat{src,dst}{pathind});
                cnt=1;
                while cnt<(hops-1)
                    cnt=cnt+1;
                    tempstring=[tempstring sprintf('%d ',pathsmat{src,dst}{pathind}(cnt)-1)];
                end
                if (writecnt==N^2-N && pathind==npaths) % the last line
                    fprintf(fID,'%s',tempstring(1:end-1));
                else
                    fprintf(fID,'%s\n',tempstring(1:end-1));
                end
                
            end
        end
    end
end

fclose(fID);