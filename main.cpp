#include <iostream>
#include <string>
#include "deamor.h"

using namespace std;

int main() {
	cout << "Mello World!" << endl;

	Memory m(10000);
	Sparse_Table st(m);

	m.print_usage();
	// Soo when testing we need to have a first element. 
	// We could write a function to "push_back" maybe...
	// But another solution is just to hack the memory...
	// but then the usage of the trees will be nonvalid, so no. 
	// Except we need to have something for recalculating usage anyway after a clean
	// So we might as well calc usage from the memory from the start.
	for(int i = 0; i < 9000; i++) {
		st.insert_after(0, i);
		m.print_usage();
	}


	st.print_stats();
	cout << "Ending" << endl;
}
