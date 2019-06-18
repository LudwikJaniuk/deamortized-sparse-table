#include <vector>
#include <cassert>
#include <string>
#include <iostream>

using namespace std;

class Memory {
	unsigned long mutable n_reads = 0;
	unsigned long n_reads_last = 0;
	unsigned long n_writes = 0;
	unsigned long n_writes_last = 0;
public:
	vector<unsigned int> data;
	vector<bool> occupied;
	Memory(size_t size) : data(size), occupied(size, false){ };

	bool is_free(size_t index) const {
		n_reads++;
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

	void print_usage() {
		cout << "\tREADS "
		     << "\tT " 
		     << n_reads
		     << "\tL "
		     << n_reads_last
		     << "\tD "
		     << n_reads - n_reads_last
		     << "\tWRITES "
		     << "\tT " 
		     << n_writes
		     << "\tL "
		     << n_writes_last
		     << "\tD "
		     << n_writes - n_writes_last
		     << endl;

		n_reads_last = n_reads;
		n_writes_last = n_writes;
	}
};
