#include <iostream>
#include <string>
#include "deamor.h"

using namespace std;

int main(int argc, char** argv) {
	size_t M_SIZE = 100000;
	size_t N_ELEMS =  9000;

	size_t L = 4;
	size_t lgL = 2;

	bool cleaning = true;
	bool verbose = false;

	// Either defaults or all manual
	if(argc >= 2) {
		if (argc < 7) {
			cout << "Invalid args" << endl;
			return 1;
		}

		if(argv[1][0] == 'c') {
			cleaning = true;
		} else if (argv[1][0] == 'n'){
			cleaning = false;
		} else {
			cout << "Invalid command line option" << endl;
			return 1;
		}

		if(argv[2][0] == 'v') {
			verbose = true;
			cout << "Verbosity on" << endl;
		} else if(argv[2][0] == 'n') {
			verbose = false;
		} else {
			cout << "Invalid command line option 2" << endl;
			return 1;
		}

		cout << (cleaning ? "c" : "nc") <<  " " << (verbose ? "v" : "nv") << " ";

		stringstream ss;
		ss << argv[3];
		ss >> L;
		cout << "L " << L << " ";

		ss.clear(); ss.str("");
		ss << argv[4];
		ss >> lgL;
		cout << "lgL " << lgL << " ";

		ss.clear(); ss.str("");
		ss << argv[5];
		ss >> M_SIZE;
		cout << "MSIZE " << M_SIZE << " ";

		ss.clear(); ss.str("");
		ss << argv[6];
		ss >> N_ELEMS;
		cout << "NELEMS " << N_ELEMS << endl;

	}
	Memory m(M_SIZE);

	Sparse_Table st(m, L, lgL);

	st.strategy = cleaning 
	 ? Sparse_Table::Strategy::CLEAN
	 : Sparse_Table::Strategy::NOCLEAN;

	st.verbose = verbose;

	m.stats_checkpoint();
	for(size_t i = 0; i < N_ELEMS; i++) {
		st.insert_after(-1, i);

		m.stats_checkpoint();
		//if (verbose) m.print_usage();
		//else if (i % 5000 == 0) cout << "." << flush;  
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

	//cout << "Ending" << endl;
}
