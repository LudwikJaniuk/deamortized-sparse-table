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
 4) adapt new algorithms DONE
 4.1) Reframe as startcleanup/continuecleanup DONE
 5) add checks for parent cleanup conflicts DONE

 6) COmpute usage efficiently
 6.0) Understainging questons
 6.0.1) What is logic gap?o
 	logical gap of a cleanup, which is the maximal empty interval extending to the left of the current write pointer.
	It is a maximal empty interval
	So reasonably, after writing on a write pointer, the write pointer cannor be part of the logical gap.
	Also I think I need to assume the endpoints are not in the gap
 6.0.2) How exactly is ecpliict usage update meant?
 	OPTION: recompute Leaf, then bubble up to make every parent update from children.
		if all children are fresh on the way there, then that's ok.
		iF they're not, is it not?
	Usage is either zero if a parent's usage is 0, otherwise it should be correct
	So if no parent is 0, then my usage should be corret
	Soo if no parent is 0, but I'm 0, I should be correct, but all my children will ignore their own values
	which makes querying a bit longer but oh well

	Explicit increment is pretty clear

	Update?
	Well, when cintnue finishes, it might be in the middle of a leaf. 
	So it might not have updated stuff on that leaf, maybe lots of writes in it or whatever. 
	And as for reads they were all inside the current logical gap, but might be spread over a really
	huge area with unboundedly many leaves,
	BUT all of those inside are not empty!
	we just can't be arsed to update them which is why we need to do it implicitly,
	this is the only thing that changes

	We can afford recomputing the leaves of the endpoint nodes, that's fine (And their parents, how?

	Does it matter that we update endpoints first and update ancestors, and only hten sero the subtrees?
	THe ancestors would read from the subtrees in the middle I think, so they might get incorrect readings form their children
	Isn't it necessary to zero the subtrees frist?	
	Yeah I'm pretty sure they do need to be zeroed first, not sure why this isn't discussed in more detail. 

	But OK, so zero all subtrees, then recompute endpoint leaves and propaagte up

	Oh, and additional detail, when the first is recomputed, and you propagate, the other still isnt.
	So temporarily the usage will be incorrect, right? 
	But then the second one also propagates and fixes it, soo all should be good. 


 6.1) When is what usage updated?
 	Update when finishing a leaf
	NOTE: 
		cleaning on Y does not change usage of Y. 
		It might redistribute usage among children of Y, but nothing leaves or enters Y. 
		Y usage before CONTINUECLEANUP and after remains the same and valid. 
		If it is valid before the insert, 
		then after the first phase it is also valid if we only do the s2 increment thing. 
		And on a leaf whether we actually just-increment or recompute, result should be the same... ((TODO maybe assertion))
		After finishing writing on a leaf we can recompute its usage
		But we donät know what has happened to the usages of all the trees we've read from
	ALGO"":
		In first phase of insertion, explicitly increment usage of all ancestors of s2
			This would increment s2's leaf by 1
			and every ancestor by 1
			Is it possible that any of them are fake-zeroed now? (?) DOn't know
		When a writepointer finishes a leaf, explicitly update the usage of all ancestors (recompute or update or?)
		When continuecleanup or startcleanup returns, "explicitly update" the usage of all ancestros of th
			the two endpoints
			of the logic gap (?)
			and zero out all the highest subtrees fully in the gap, thereby implizityly zeroing the gap
		When checking usage need to check higher-ups for 0
 6.3) DO we need to keep the freshness score? I think it is a good assertion
 	// I think it won't really work anymore? We explicitly have situations now where itäs violated
	// TODO assertion for correct usages throughout tho
 6.4) Implement usage efficianlty

 6.4.1) Usage query: check for 0 parent DONE
 6.4.2) Explicit increment s2  DONE
 6.4.3) Recalc on finished leaf and propagate  DONE
	Thsi probably leaves an error too but it is the same as in the zro case and will be corrected for at the end
 6.4.4) After end of continue
 6.4.4.1) zero mid subtrees 
 6.4.4.2) recalc leaf of one endpoint and proparagte
 6.4.4.3) ditto with other






 7) Other efficiencies
 7.1) FInding next occupied slot to the ledt
 7.2) finding next usable slot to the left


MISTAKE SOMEOWHEWE with usage

Seems like buffers we're skipping while cleaning, well since they are empty we are not setting them to 0 even tho they should be.
And not their parents either...

THink I need to make the presentation now.

I think to maintain the usage correctly, when cleaning algo hops over a buffer, that buffer need to be cleared too. 
Q Is this still constant in the number of leaves went over?
We're going over alpha leaves...
When finisheing a leaf, we are updating its value and all of its ancestors
What's missing would be that while traversing up the ancestors on a finished leaf, 
	as long as we're under y, 
	any buffer is known to be clean! Right?
	So zero them on the way there!...
	When finisheing a leaf, we are updating its value and all of its ancestors
	What's missing would be that while traversing up the ancestors on a finished leaf, 
		as long as we're under y, 
		any buffer is known to be clean! Right?
		So zero them on the way there!

We're reading left of the final readpointer!!!
final readpointer is calculated incorrectly, itäs recalculated after thew write which maeks it same as w.
need to save its last value instead.
