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
		Node* parent;
		size_t data_index;
		size_t data_length;
	private:
		size_t usage = 0;
	public:

		Node(Node *p = nullptr) : parent(p){};

		size_t Usage() { return usage; };
		bool is_leaf() { return (bool)left; }

		void init(size_t level, size_t index) {
			cout << level;
			left = nullptr;
			right = nullptr;
			buffer = nullptr;
			data_index = index;

			if(level == 0) {
				data_length = L;
			} else {
				left = new Node(this);
				left->init(level-1, index);

				right = new Node(this);
				right->init(level-1, index + left->data_length);

				data_length = left->data_length + right->data_length;

				if (level >= lgL) {
					buffer = new Node(this);
					buffer->init(level-lgL, index + data_length);
					data_length += buffer->data_length;
				} 
			}
		}

		bool index_in_range(size_t index) {
			return index >= data_index && index < data_index + data_length;
		}

		// Returns the child covering the index. One step in the iteration towards that leaf. 
		// Must always be called with a child inrange.
		// If leaf, will return null.
		Node *child_over(size_t index) {
			assert(index_in_range(index));

			if(!left) return nullptr; // we're a leaf

			assert(right);
			if(left->index_in_range(index)) return left;
			if(right->index_in_range(index)) return right;

			assert(buffer);
			assert(buffer->index_in_range(index));
			return buffer;
		}

		Node *leaf_over(size_t index) {
			Node * n;
			for(n = this; n->left; n = n->child_over(index)) {}
			return n;
		}


		void change_usage(int diff) {
			assert(is_leaf());
			assert(diff == 1 || diff == -1); // Nothing else makes sense

			for(Node* p = this; p; p = p->parent) {
				int new_usage = (int)p->usage + diff;
				assert(0 <= new_usage && (size_t)new_usage < p->data_length);
				p->usage = (size_t)new_usage;
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
	void print_stats() {
		cout << "tree usage: " << tree.Usage() << endl;
	}

private:
	void clean_if_necessary(size_t last_inserted_index);
	size_t first_free_right_of(size_t index);
	void shuffle_right(size_t left_border, size_t right_free);
};

void Sparse_Table::insert_after(size_t index, unsigned value) {
	size_t free_spot = first_free_right_of(index);
	if(free_spot != index+1) shuffle_right(index+1, free_spot);
	m.write(index+1, value);
	
	Node *usage_leaf = tree.leaf_over(free_spot);
	assert(usage_leaf);

	usage_leaf->change_usage(1);

	// See if we need cleaning
	// Optionally clean
}

void Sparse_Table::clean_if_necessary(size_t last_inserted_index) {
	Node* leaf = tree.leaf_over(last_inserted_index);
	assert(leaf);


}

// Does not modify tree
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
