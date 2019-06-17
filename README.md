# deamortized-sparse-table

Soo, we want to implement at least the amortized version of the deamortized version of sparse tables using the asymetric ternary tree.
Do we want some kind of simplifying library? or rather, interface?
Like, "store X at Y"? 
We can already do that with vector of course, but we can't add logging and measuring. That's what an interface would be good for. 
Maybe a class called "memory".
We initiate it, then can "read" and "write". 
and it could either log stuff on each use, or batch the stats and output them later.
