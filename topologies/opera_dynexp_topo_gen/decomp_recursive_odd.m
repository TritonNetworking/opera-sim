function [Asol,sol,itercnt]=decomp_recursive_odd(N,Niter,strtmat,lvl,Asol,itercnt,pflag)

rng('shuffle');

lvl=lvl+1; % update recursion level

if lvl==N
    Niter=1;
end

[ipool,jpool]=find(strtmat==1);
[Npool0,~]=size(ipool);

% 9/3/18 - OPTIMIZATION NOTE: must always pick exactly one diagonal element
diaginds=[]; % get indices of diagonal elements
for a=1:Npool0
    if ipool(a) == jpool(a)
        diaginds=[diaginds a];
    end
end

sol=0; % do we have a solution?
stp=0; % have we reached the maximum number of iterations?
cnt=0; % keep track of how many iterations we've done
while sol==0 && stp==0
    
    inpool=ones(1,Npool0); % if inpool(i)==1, element i is still available
    A=zeros(N); % initialize adjacency matrix to zero
    izeroed=zeros(N,1); % keep track of which rows/columns we've zeroed
    donepop=0; % are we done populating this permutation?
    
    % Get the choice of diagonal out of the way first
    if ~isempty(diaginds)
        
        % pick one
        ndiags=length(diaginds);
        diaginds2=diaginds(randperm(ndiags,1));
            
        ipick=ipool(diaginds2);
        inpool(diaginds2)=0; % remove this element from pool
        
        % update A
        A(ipick,ipick)=1;
        
        % delete the elements of ipool, jpool
        temp=sum(A,2);
        zinds=find(temp==1); % row/col inds to zero
        if izeroed(zinds)==0
            inds2z=find(ipool==zinds);
            if isempty(inds2z)==0
                inpool(inds2z)=0;
            end
            inds2z=find(jpool==zinds);
            if isempty(inds2z)==0
                inpool(inds2z)=0;
            end
            izeroed(zinds)=1; % record that we zeroed this index
        end
        
        % remove the rest of the diagonals for this round
        inpool(diaginds)=0;
        
    end
    
    while donepop==0
        Npool=sum(inpool);
        
        inds=find(inpool>0);
        indpick=randi([1 Npool]); % get random integer
        
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
    
    cnt=cnt+1; % update number of iterations
    if cnt==Niter
        stp=1; % stop if we've reached maximum number of iterations
    end
    
    itercnt=itercnt+1;
    if mod(itercnt,5000)==0
        if pflag==1
            temp=sprintf('Iteration: %d, Level: %d ',itercnt,lvl);
            fprintf('%s\n',temp);
        end
    end
    
    if min(sum(A))==1 % we have a valid symmetric permutation

        Asol{lvl}=A; % save the solution
        
        if lvl<N % there are more levels to go...
            
            % call the next level
            temp=strtmat-A;
            temp1=tril(ones(N));
            temp1(logical(diag(ones(1,N),0)))=0;
            temp(logical(temp1))=0; % zero out lower part
            [Asol,sol1,itercnt]=decomp_recursive_odd(N,Niter,temp,lvl,Asol,itercnt,pflag);
            
            if sol1==1 % the lower level succeeded
                sol=1;
            end
            
        else % this is the last level of recursion
            sol=1;
        end
        
    end
end


