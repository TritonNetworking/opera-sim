function [A]=const_rama(N,u,rama)

% N = number of endpoints
% u = number of endpoint uplinks
% rama = ramanujan bound

% construct starting matrix:
utmat=triu(ones(N));
utmat(logical(diag(ones(1,N))))=0;
strtmat=utmat;

sol=0; % loop until we find a set of disjoint permutations that's ramanujan
while sol==0
    
    [ipool,jpool]=find(strtmat==1);
    [Npool,~]=size(ipool);
    inpool=ones(1,Npool); % if inpool(i)==1, element i is still available
    A=zeros(N); % initialize adjacency matrix to zero
    npop=u*N/2; % number of elements to populate
    izeroed=zeros(N,1); % keep track of which rows/columns we've zeroed
    
    for x=1:npop
        Npool=sum(inpool);
        
        if Npool==0 % in case we made bad choices and there's not a solution
            break;
        end
        
        inds=find(inpool>0);
        indpick=randi([1 Npool]); % get random integer
        
        ipick=ipool(inds(indpick));
        jpick=jpool(inds(indpick));
        inpool(inds(indpick))=0; % remove this element from pool
        
        % update A
        A(ipick,jpick)=1;
        A(jpick,ipick)=1;
        
        % check if we need to delete any elements of ipool, jpool
        temp=sum(A,2);
        zinds=find(temp==u); % row/col inds to zero
        if isempty(zinds)==0
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
    end
    
    temp=sort(abs(eig(A)));
    lambda=temp(end-1); % second eigenvalue
    
    if min(sum(A))==u && lambda<=rama % we have a solution
        
        sol=1;
        
    end
end


