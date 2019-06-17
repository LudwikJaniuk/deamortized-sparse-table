#include <iostream>
#include <string>
#include "deamor.h"

using namespace std;

int main() {
	cout << "Mello World!" << endl;

	Memory m(10000);
	Sparse_Table st(m);

	m.print_usage();
	for(int i = 0; i < 9000; i++) {
		st.insert_after(0, i);
		m.print_usage();
	}


	st.print_stats();
	cout << "Ending" << endl;
}
