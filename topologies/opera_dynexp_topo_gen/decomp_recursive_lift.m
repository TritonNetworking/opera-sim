function [Asol,sol,itercnt]=decomp_recursive_lift(N,Niter,strtmat,lvl,Asol,itercnt,pflag)

rng('shuffle');

lvl=lvl+1; % update recursion level

if lvl==N
    Niter=1;
end

% debug:
% temp=sprintf('Starting iteration: %d, Level: %d ',itercnt,lvl);
% fprintf('%s\n',temp);

[ipool,jpool]=find(strtmat==1);
[Npool0,~]=size(ipool);

% 7/11/18 - OPTIMIZATION NOTE: must always pick diagonal elements in
% pairs (N is assumed to be even) for the matrix to work.
diaginds=[]; % get indices of diagonal elements
for a=1:Npool0
    if ipool(a) == jpool(a)
        diaginds=[diaginds a];
    end
end

% 7/12/18 - OPTIMIZATION NOTE: The second to last iteration has to finish
% in one step or it will never work...

sol=0; % do we have a solution?
stp=0; % have we reached the maximum number of iterations?
localcnt=0; % keep track of how many local iterations we've done
while sol==0 && stp==0
    
    itercnt=itercnt+1;
%     if pflag==1
%         if mod(itercnt,1)==0
%             temp=sprintf('Iteration: %d, Level: %d ',itercnt,lvl);
%             fprintf('%s\n',temp);
%         end
%     end
    
    % init for this local iteration
    inpool=ones(1,Npool0); % if inpool(i)==1, element i is still available
    A=zeros(N); % initialize adjacency matrix to zero
    izeroed=zeros(N,1); % keep track of which rows/columns we've zeroed
    donepop=0; % are we done populating this permutation?
    
    % Get the diagonals out of the way first, in pairs
    if ~isempty(diaginds)
        
        % pick them
        ndiags=length(diaginds);
        diaginds2=diaginds(randperm(ndiags,2));
        
        for a=1:2
            
            ipick=ipool(diaginds2(a));
            inpool(diaginds2(a))=0; % remove this element from pool
            
            % update A
            A(ipick,ipick)=1;
            
            % delete the elements of ipool, jpool
            temp=sum(A,2);
            zinds=find(temp==1); % row/col inds to zero
            [Nz,~]=size(zinds);
            for y=1:Nz
                if izeroed(zinds(y))==0
                    inds2z=find(ipool==zinds(y));
                    if isempty(inds2z)==0
                        inpool(inds2z)=0;
                    end
                    inds2z=find(jpool==zinds(y));
                    if isempty(inds2z)==0
                        inpool(inds2z)=0;
                    end
                    izeroed(zinds(y))=1; % record that we zeroed this index
                end
            end
        end
        
        % remove the rest of the diagonals for this round
        inpool(diaginds)=0;
        
        if sum(izeroed)==N
            donepop=1; % this only happens if N==2
        end
        
    end
    
    % Diagonal elements can't show up here:
    while donepop==0
        
        Npool=sum(inpool);
        indpick=randi([1 Npool]);
        inds=find(inpool>0);
        ipick=ipool(inds(indpick));
        jpick=jpool(inds(indpick));
        inpool(inds(indpick))=0; % remove this element from pool
        
        % update A
        A(ipick,jpick)=1;
        A(jpick,ipick)=1;
        
        % delete the elements of ipool, jpool
        temp=sum(A,2);
        zinds=find(temp==1); % row/col inds to zero
        [Nz,~]=size(zinds);
        for y=1:Nz
            if izeroed(zinds(y))==0
                inds2z=find(ipool==zinds(y));
                if isempty(inds2z)==0
                    inpool(inds2z)=0;
                end
                inds2z=find(jpool==zinds(y));
                if isempty(inds2z)==0
                    inpool(inds2z)=0;
                end
                izeroed(zinds(y))=1; % record that we zeroed this index
            end
        end
        
        if sum(inpool)==0 % check if we're done
            donepop=1;
        end
        
    end
    
    localcnt=localcnt+1; % update number of iterations
    if localcnt==Niter || lvl==N % if we don't finish the last level in 1 iteration, go back
        stp=1; % stop if we've reached maximum number of iterations
        
        % debug:
%         if lvl<N
%             % debug: couldn't find a solution here...
%             temp=sprintf('GAVE UP @ Iteration: %d, Level: %d ',itercnt,lvl);
%             fprintf('%s\n',temp);
%             temp=zeros(N);
%             for a=1:lvl-1
%                 temp=temp+Asol{a};
%             end
%             temp=ones(N)-temp;
%             fprintf(sprintf('    %d elements left to assign\n',min(sum(temp))));
%             save('temp.mat','temp');
%             figure;
%             spy(temp);
%             title('empty slots');
%         end
        
    end
    
    if min(sum(A))==1 % we have a valid symmetric permutation
        
        Asol{lvl}=A; % save the solution
        
        % debug:
%         if lvl==9
%             figure;
%             spy(strtmat+strtmat.');
%         end
%         
%         if lvl==9 || lvl==10
%             figure;
%             spy(A);
%             title(sprintf('lvl = %d',lvl));
%         end
        
        % debug:
        %         figure;
        %         hold on;
        %         c=colormap(jet(N));
        %         for a=1:lvl
        %             spy(Asol{a});
        %         end
        %         h=get(gca,'children');
        %         for a=1:lvl
        %             h(a).Color=c(lvl+1-a,:);
        %         end
        
        if lvl<N % there are more levels to go...
            
            % call the next level
            temp=strtmat-A;
            temp1=tril(ones(N));
            temp1(logical(diag(ones(1,N),0)))=0;
            temp(logical(temp1))=0; % zero out lower part
            [Asol,sol1,itercnt]=decomp_recursive_lift(N,Niter,temp,lvl,Asol,itercnt,pflag);
            
            if sol1==1 % the lower level succeeded
                sol=1;
            end
            
        else % this is the last level of recursion
            
            % debug:
%             fprintf(sprintf('lvl = %d finished at iteration %d',lvl,itercnt));
            
            sol=1;
        end
    end
    
end


