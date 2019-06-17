#include <cassert>
#include "memory.h"

class Sparse_Table {
	Memory& m;
public:
	Sparse_Table(Memory& mem) : m(mem) {};
	void insert_after(size_t index, unsigned value);
	void delete_at(size_t index) {
		m.delete_at(index);
	};
private:
	size_t first_free_right_of(size_t index);
	void shuffle_right(size_t left_border, size_t right_free);
};

void Sparse_Table::insert_after(size_t index, unsigned value) {
	size_t free_spot = first_free_right_of(index);
	if(free_spot != index+1) shuffle_right(index+1, free_spot);
	m.write(index+1, value);
	// See if we need cleaning
	// Optionally clean
}

void Sparse_Table::shuffle_right(size_t left_border, size_t right_free) {
	assert(right_free > left_border);
	do {
		m.write(right_free, m.read(right_free-1));
		right_free--;
	} while(right_free > left_border);
	m.delete_at(left_border);
}

size_t Sparse_Table::first_free_right_of(size_t index) {
	while(true) {
		index++;
		if (m.is_free(index)) return index;
	}
}
