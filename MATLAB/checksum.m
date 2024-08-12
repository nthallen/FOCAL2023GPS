%%
str = 'PMTK220,100';
nstr = double(str);
acc = nstr(1);
for i=2:length(nstr); acc = bitxor(acc,nstr(i)); end
fprintf(1, '%s\n', [ '$' str '*' dec2hex(acc) ]);

