function []=write_to_htsim_file(flowmat,filename)

fprintf('Writing to htsim file...\n');
tic;

fID=fopen([filename '.htsim'],'w');

[nflows,~]=size(flowmat);

% -1 because servers are indexed from zero in htsim
for a=1:nflows
    if a~=nflows
        fprintf(fID,'%.0f %.0f %.0f %.0f\n',flowmat(a,1)-1,flowmat(a,2)-1,flowmat(a,3),flowmat(a,4));
    else
        fprintf(fID,'%.0f %.0f %.0f %.0f',flowmat(a,1)-1,flowmat(a,2)-1,flowmat(a,3),flowmat(a,4));
    end;
end;

fclose(fID);

t=toc;
fprintf(sprintf('    Finished in %.2f seconds\n',t));