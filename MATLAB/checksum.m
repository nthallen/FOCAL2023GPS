%%
% str = 'PMTK220,100';
%str = 'Q,+000.30,+000.21,-000.43,M,+345.68,+023.51,00,';
str = 'PMTK251,9600';
nstr = double(str);
acc = nstr(1);
for i=2:length(nstr); acc = bitxor(acc,nstr(i)); end
fprintf(1, '%s\n', [ '$' str '*' dec2hex(acc) ]);

