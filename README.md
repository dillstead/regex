# regex
Regular Expression Search Algorithm 
## Description
An implemetation of Ken Thompson's regular expression search algorithm detailed in his 1968 paper, "Regular Expression Search Algorithm".  The paper can be found [here](http://doi.acm.org/10.1145/363347.363387).

Ken Thompson's implemented the compiled regular expression as a block of IBM 7094 assembly code that, when given a string, searches for all matches of the regular expression in the string.  This implemention compiles the regular expression into a relocatable block of 32-bit ARM assembly.
