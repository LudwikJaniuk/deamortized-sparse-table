#include <iostream>
#include <string>
#include "deamor.h"

using namespace std;

int main(int argc, char** argv) {
	cout << "Mello World!" << endl;

	size_t M_SIZE = 100000;
	size_t N_ELEMS =  9000;

	size_t L = 4;
	size_t lgL = 2;
	if(argc >= 4) {
		if(argv[3][0] == 'l') {
			cout << "Manually set L" << endl;
			cin >> L;
			cout << "Manually set lgL (to the 2-log of L please)" << endl;
			cin >> lgL;
		} else if(argv[3][0] == 'n') {
			cout << "Leaving L lgL as in source code" << endl;
		} else {
			cout << "Invalid command line option 3" << endl;
		}
	}

	if(argc >= 5) {
		if(argv[4][0] == 'd') {
			cout << "Manually set M_SIZE" << endl;
			cin >> M_SIZE;
			cout << "Manually set N_ELEMS" << endl;
			cin >> N_ELEMS;
		} else if(argv[3][0] == 'n') {
			cout << "Leaving M_SIZE N_ELEMS as in source code" << endl;
		} else {
			cout << "Invalid command line option 3" << endl;
		}
	}

	Memory m(M_SIZE);

	Sparse_Table st(m, L, lgL);



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
		} else if(argv[2][0] == 'n') {
			verbose = false;
			cout << "Verbosity off" << endl;
		} else {
			cout << "Invalid command line option 2" << endl;
		}
	}
	st.verbose = verbose;

	m.stats_checkpoint();
	for(size_t i = 0; i < N_ELEMS; i++) {
		st.insert_after(-1, i);

		m.stats_checkpoint();
		if (verbose) m.print_usage();

		else if (i % 1000 == 0) cout << "." << flush;  
	}
	cout << endl;
	m.print_summary();
	if(verbose)st.print_stats();

	unsigned int n = 0;
	for(size_t i = 0; i < N_ELEMS; i++) {
		if(m.is_free(i)) continue;

		if(m.data[i] != N_ELEMS-n-1) {

			cout << "INCORRECT" << " " << i << " " << m.data[i] << endl;
			size_t step = N_ELEMS / 100;
			for(size_t i = 0; i < N_ELEMS; i++) {
				if(i % step == 0) {
					cout << i << " " << m.data[i] << endl;
				}
			}
			break;
		}

		n++;
	}

	cout << "Ending" << endl;
}
