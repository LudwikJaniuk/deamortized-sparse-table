#include <cassert>
#include <cmath>
#include <string>
#include <iostream>
#include "memory.h"

using namespace std;


class Sparse_Table {
	Memory& m;
	static const size_t L = 4;
	static const size_t lgL = 2;
	size_t depth = 0;
	size_t capacity = 0;

	struct Node {
		Node* left;
		Node* right;
		Node* buffer;
		size_t data_index;
		size_t data_length;

		void init(size_t level, size_t index) {
			cout << level;
			left = nullptr;
			right = nullptr;
			buffer = nullptr;
			data_index = index;

			if(level == 0) {
				data_length = L;
			} else {
				left = new Node();
				left->init(level-1, index);

				right = new Node();
				right->init(level-1, index + left->data_length);

				data_length = left->data_length + right->data_length;

				if (level >= lgL) {
					buffer = new Node();
					buffer->init(level-lgL, index + data_length);
					data_length += buffer->data_length;
				} 
			}
		}
	};

	Node tree;

	void init_tree() {
		// Determine capacity
		vector<size_t> level_capacity;
		for(depth = 0; ; depth++) {

			size_t required_mem = 0;
			if (depth == 0) {
				required_mem = L;
			} else if (depth < lgL) {
				required_mem = 2 * level_capacity[depth-1];
			} else {
				required_mem = 2 * level_capacity[depth-1] + level_capacity[depth-lgL];
			}

			if(required_mem > m.data.size()) {
				depth--;
				break;
			}

			level_capacity.push_back(required_mem);
			capacity = required_mem;
		}

		cout << "depth is on " << depth << endl;
		cout << "capacity is on " << capacity << endl;
		tree.init(depth, 0);
		cout << endl;
		cout << "DATA LEN: " << tree.data_length << endl;

	}
public:
	Sparse_Table(Memory& mem) : m(mem) {
		init_tree();
	};
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
