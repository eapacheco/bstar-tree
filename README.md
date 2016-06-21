# bstar-tree

Developed by Eduardo A. Pacheco
"For of him, and through him, and to him, are all things:
to whom be glory for ever. Amen." (Romans 11:36 KJV)
June 21, 2016 - Brazil

A B-Tree variant that guarantees that at least two thirds of each node, but the root, is filled (B*-Tree)
Every implementation detail may be found on the intern and extern documentation.

Every resource that isn't available in English may be translated:
let me know if you need it, so I may speeded it up for you :)

------------------------------------------

(bs-tree.h header)
Formal definition: 
The B*-Tree index with a degree m is the defined as:
1.	Every page (or node) in it has the following format
	<P1, <C1,PR1>, P2, <C2,PR2>, ..., Pq-1, <Cq-1,PRq-1>, Pq>, where (q <= m) and
	Each Ci (1 <= i <= q-1) is a search key
	Each PRi (1 <= i <= q-1) is a reference field pointing to the record at the
		data file
	Each Pj (1 <= j <= q) points to a subtree or has a NULL value, if it has no subtree
2.	Inside each page
	C1 < C2 < ... < Cq-1.
3.	Every key X in the subtree at Pi, about Ci keys in parent 
	Ci-1 < X < Ci (1 < i < q)
	X < Ci (i = 1)
	Ci-1 < X (i = q)
4.	Each page has a maximum of m descendents, but the root
5. 	Each page, but the root, has at least (2m-1)/3 descendents
6. 	Root has at least two descendents, unless it's also a leaf
7.	Every leaf is at the same height/level
8. 	A non-leaf page has n descendents and n-1 keys
9. 	A leaf page has at least ⎣2(m-1)/3⎦ keys and a maximum of m-1 keys (occupancy rate)
10.	The root page is peculiar:
	Double-sized: has a maximum of 2m-2 keys and 2m-1 descendents
	Has no lower bounds for occupancy: it may have less than ⎣2(m-1)/3⎦, including zero.

More information, specific of this implementation, may be found
on the attached .pdf file at: https://github.com/eapacheco/bstar-tree (PORTUGUESE ONLY)

Further information may be found at:
https://en.wikipedia.org/wiki/B-tree
https://en.wikipedia.org/wiki/B-tree#Variants
