# deamortized-sparse-table

Soo, we want to implement at least the amortized version of the deamortized version of sparse tables using the asymetric ternary tree.
Do we want some kind of simplifying library? or rather, interface?
Like, "store X at Y"? 
We can already do that with vector of course, but we can't add logging and measuring. That's what an interface would be good for. 
Maybe a class called "memory".
We initiate it, then can "read" and "write". 
and it could either log stuff on each use, or batch the stats and output them later.

NOCLEAN:
	READS 	T 81055440	L 81037441	D 17999	WRITES 	T 40504501	L 40495501	D 9000

CLEAN:
	READS 	T 298079672	L 298079641	D 31	WRITES 	T 463537	L 463521	D 16

TODOLIST:
	 * Test different page size DONE
	 * Reduce number of free checks? DONE
	 * Implement deamortization 

DEAMORTIZATION
 0) measure our pause times
 1) break cleanup into chunks
 2) start cleanups at lower treshold
 3) incorporate slack slots
 4) add checks for parent cleanup conflicts
