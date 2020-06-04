function []=lift_construction(Nbase,N)


% NOTE: this version enforces symmetry in the lifted matrix

% inputs:

% Nbase=75; % number endpoints in starting graph
% N=1200; % number endpoints in graph to be constructed

% ------

load(sprintf('Decomp_N=%d.mat',Nbase));

LF=N/Nbase; % lift factor

% generate a bunch of LF-sized-permutation decompositions that work together
Ntries=10;
P_sets=cell(1,Ntries);
Niter=1000;
strtmat=triu(ones(LF));
lvl=0;
Asol=cell(1,LF);
itercnt=0;
pflag=0;
for iter=1:Ntries
    [P_sets{iter},~,~]=decomp_recursive_lift(LF,Niter,strtmat,lvl,Asol,itercnt,pflag);
end

Plifted=cell(1,N); % generated permutations
for perm=1:Nbase % sweep permutations
    for iter=1:LF
        Plifted{(perm-1)*LF+iter}=sparse(zeros(N));
    end
    [s,d]=find(P{perm}==1);
    [s_done,d_done]=deal([]);
    Ndone=0;
    for elem=1:Nbase % sweep non-zero elements
        sym_ind=0; % is the symmetric element already done?
        for i=1:Ndone
            if (s(elem) == d_done(i)) && (d(elem) == s_done(i))
                % we've already seen the symmetric element
                sym_ind=i;
            end
        end
        if sym_ind==0
            
            s_done=[s_done s(elem)];
            d_done=[d_done d(elem)];
            Ndone=Ndone+1;
            
            set=randi([1,Ntries]); % choose a set randomly
            order=randperm(LF); % choose an ordering randomly
            for iter=1:LF
                Plifted{(perm-1)*LF+iter}((s(elem)-1)*LF+1:s(elem)*LF,(d(elem)-1)*LF+1:d(elem)*LF)=P_sets{set}{order(iter)};
            end
        else
            for iter=1:LF
                Plifted{(perm-1)*LF+iter}((s(elem)-1)*LF+1:s(elem)*LF,(d(elem)-1)*LF+1:d(elem)*LF)= ...
                    Plifted{(perm-1)*LF+iter}((d(elem)-1)*LF+1:d(elem)*LF,(s(elem)-1)*LF+1:s(elem)*LF);
            end
        end
    end
end

% figure;
% hold on;
% for perm=1:N
%     spy(Plifted{perm});
%     pause(.1);
% end

P=Plifted; % save the lifted version as `P`

save(sprintf('Lifted_Decomp_Nbase=%d_N=%d.mat',Nbase,N),'P');











