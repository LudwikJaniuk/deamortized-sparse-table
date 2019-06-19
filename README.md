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

COROLLARY 5.2
alpha >= 9, L >= 32 in that case

DEAMORTIZATION
 0) measure our pause times DONE
 0.1) Understand equlity and leftheaviness DONE
 0.2) Maintain leaf mapping explicitly DONE
 1) break cleanup into chunks DONE
 2) start cleanups at lower treshold DONE
 3) incorporate slack slots DONE
 3.1) adapt old code DONE
 4) adapt new algorithms
 4.1) Reframe as startcleanup/continuecleanup
 4.2) Maintain usage more relaxed
 5) add checks for parent cleanup conflicts

AFter lots of coding, finding the bugs bery hard. Going to bet.
