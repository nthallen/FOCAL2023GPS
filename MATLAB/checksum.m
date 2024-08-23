%%
% str = 'PMTK220,100';
%str = 'Q,+000.30,+000.21,-000.43,M,+345.68,+023.51,00,';
%str = 'PMTK251,9600';
%str = 'PMTK251,14400';
%str = 'PMTK220,1000';
%str = 'PMTK220,74';
%str = 'PMTK220,70';
%str = 'PMTK220,50';
%str = 'PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0';
%str = 'PMTK314,0,1,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0';
str = 'PMTK314,0,1,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0';
nstr = double(str);
acc = nstr(1);
for i=2:length(nstr); acc = bitxor(acc,nstr(i)); end
fprintf(1, '%s\n', [ '$' str '*' dec2hex(acc) ]);

