#include <vector>
#include <cassert>
#include <string>
#include <iostream>

using namespace std;

class Memory {
	unsigned long mutable n_reads = 0;
	unsigned long n_reads_last = 0;
	unsigned long n_reads_worst_diff = 0;
	unsigned long n_writes = 0;
	unsigned long n_writes_last = 0;
	unsigned long n_writes_worst_diff = 0;
	unsigned long mutable n_free_checks = 0;
	unsigned long n_free_checks_last = 0;
	unsigned long n_free_checks_worst_diff = 0;
public:
	vector<unsigned int> data;
	vector<bool> occupied;
	Memory(size_t size) : data(size), occupied(size, false){ };

	bool is_free(size_t index) const {
		n_free_checks++;
		return !occupied[index];
	}

	unsigned read(size_t index) const {
		assert(index < data.size());
		assert(occupied[index]);

		n_reads++;

		return data[index];
	}

	void write(size_t index, unsigned value) {
		assert(index < data.size());
		data[index] = value;
		occupied[index] = true;

		n_writes++;
	}

	void delete_at(size_t index) {
		assert(occupied[index]);
		occupied[index] = false;
	}

	void stats_checkpoint() {
		unsigned long r_d = n_reads - n_reads_last;
		unsigned long w_d = n_writes - n_writes_last;
		unsigned long c_d = n_free_checks - n_free_checks_last;

		n_reads_worst_diff = max(n_reads_worst_diff, r_d);
		n_writes_worst_diff = max(n_writes_worst_diff, w_d);
		n_free_checks_worst_diff = max(n_free_checks_worst_diff, c_d);

		n_reads_last = n_reads;
		n_writes_last = n_writes;
		n_free_checks_last = n_free_checks;
	}

	void print_usage() {
		cout << "\tREADS "
		     << "\tT " 
		     << n_reads
		     << "\tD "
		     << n_reads - n_reads_last
		     << "\tWRITES "
		     << "\tT " 
		     << n_writes
		     << "\tD "
		     << n_writes - n_writes_last
		     << "\tfree_checks "
		     << "\tT " 
		     << n_free_checks
		     << "\tD "
		     << n_free_checks - n_free_checks_last
		     << endl;
	}

	void print_summary() {
		cout << "R, lg10R, W, lg10W, C, lg10C, wdR, lg10wdR, wdW, lg10wdW, wdC, lg10wdC" << endl;
		cout << n_reads << ", " << log10(n_reads) << ", " 
		     << n_writes  << ", " << log10(n_writes) << ", " 
		     << n_free_checks << ", " << log10(n_free_checks) << ", "
		     << n_reads_worst_diff << ", " << log10(n_reads_worst_diff) << ", "
		     << n_writes_worst_diff  << ", " << log10(n_writes_worst_diff ) << ", "
		     << n_free_checks_worst_diff  << ", " << log10(n_free_checks_worst_diff ) << endl;
		/*
		cout << "R " << n_reads << " log10 " << log10(n_reads) << endl;
		cout << "wd " << n_reads_worst_diff << " log10 " << log10(n_reads_worst_diff) << endl;
		cout << "W " << n_writes << " log10 " << log10(n_writes) << endl;
		cout << "wd " << n_writes_worst_diff  << " log10 " << log10(n_writes_worst_diff ) << endl;
		cout << "C " << n_free_checks << " log10 " << log10(n_free_checks) << endl;
		cout << "wd " << n_free_checks_worst_diff  << " log10 " << log10(n_free_checks_worst_diff ) << endl;
		*/
	}
};
