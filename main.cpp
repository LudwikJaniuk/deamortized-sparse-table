#include <iostream>
#include <string>
#include "deamor.h"

using namespace std;

int main(int argc, char** argv) {
	cout << "Mello World!" << endl;

	const size_t M_SIZE = 100000;
	const size_t N_ELEMS =  9000;
	Memory m(M_SIZE);

	Sparse_Table st(m);

	if(argc >= 2) {
		if(argv[1][0] == 'c') {
			st.strategy = Sparse_Table::Strategy::CLEAN;
			cout << "CLEANING ON" << endl;
		} else if (argv[1][0] == 'n'){
			st.strategy = Sparse_Table::Strategy::NOCLEAN;
			cout << "CLEANING off" << endl;
		} else {
			cout << "Invalid command line option" << endl;
		}
	}

	bool verbose = false;
	if(argc >= 3) {
		if(argv[2][0] == 'v') {
			verbose = true;
			cout << "Verbosity on" << endl;
		} else {
			cout << "Invalid command line option 2" << endl;
		}
	}

	st.verbose = verbose;

	m.print_usage();
	for(size_t i = 0; i < N_ELEMS; i++) {
		st.insert_after(-1, i);
		if (verbose || i % 1000 == 0) m.print_usage();
	}
	m.print_usage();


	st.print_stats();
	cout << "Ending" << endl;
}
