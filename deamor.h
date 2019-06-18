#include <cassert>
#include <cmath>
#include <string>
#include <sstream>
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
		Node* parent = nullptr; // Set before init

		// Level-wise, not always topologically
		Node* l_sibling = nullptr;
		Node* r_sibling = nullptr;

		Node* left = nullptr;
		Node* right = nullptr;
		Node* buffer = nullptr;

		size_t data_index;
		size_t data_length;
		size_t primary_capacity;
		size_t usable_capacity;
		size_t m_level = 0;
		size_t level_offset = 0;
	private:
		size_t usage = 0;
		const Memory& m;
		Sparse_Table& st;
		bool initialized = false;
	public:
		Node(Node *p, const Memory& mem, Sparse_Table& tab) 
			: parent(p)
			, m(mem)
			, st(tab)
		{};

		size_t Usage() { return usage; };
		bool is_parent() { return left; }
		bool is_leaf() { return !is_parent(); }

		void init(size_t level, size_t index) {
			data_index = index;
			m_level = level;

			if(parent == nullptr) {
				assert(data_index == 0);
				st.level_leftmost = vector<Node*>(level+1);
				st.level_rightmost = vector<Node*>(level+1);
			}

			if(data_index == 0) {
				level_offset = 0;
				st.level_leftmost[level] = this;
				st.level_rightmost[level] = this; // Assumes we're always the rightmost right now to be initialized
			} else {
				assert_parenthood();

				assert(st.level_rightmost[level]);
				assert(st.level_rightmost[level]->r_sibling == nullptr);

				Node* ls = st.level_rightmost[level];
				ls->r_sibling = this;
				this->l_sibling = ls;
				assert(l_sibling->initialized);
				assert(l_sibling->m_level == m_level);

				st.level_rightmost[level] = this;
				level_offset = l_sibling->level_offset + 1;
			}

			if(m_level == 0) {
				data_length = L;
				primary_capacity = L;
				usable_capacity = L;
			} else {
				left = new Node(this,  m, st);
				left->init(m_level-1,  index);

				right = new Node(this, m, st);
				right->init(m_level-1, index + left->data_length);

				data_length = left->data_length + right->data_length;
				primary_capacity = left->primary_capacity + right->primary_capacity;
				usable_capacity = primary_capacity;

				if (m_level >= lgL) {
					buffer = new Node(this, m, st);
					buffer->init(m_level-lgL, index + data_length);
					data_length += buffer->data_length;
					usable_capacity += buffer->primary_capacity;
				} 
			}

			initialized = true;
		}


		// We can't require that parent is initialized or complete since this is used in initialization to climb upwards
		void assert_parenthood() {
			assert(parent);
			assert(this);
			assert(parent->left);
			assert(parent->left == this
			    || (parent->right && parent->right == this && (m_level+1 == parent->m_level))
			    || (parent->right && parent->buffer && parent->buffer == this && (m_level+lgL == parent->m_level)));
		}

		void recalculate_usage() {
			usage = 0;
			if(is_leaf()) {
				size_t past_end = data_index + data_length;
				for(size_t i = data_index; i < past_end; i++) {
					usage += (size_t)(!m.is_free(i));
				}
				return;
			}

			assert(left);
			left->recalculate_usage();
			usage += left->Usage();

			if(!right) { assert(!buffer); return; }

			right->recalculate_usage();
			usage += right->Usage();

			if(!buffer) { return; }

			buffer->recalculate_usage();
			usage += buffer->Usage();
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
			assert(n->left == nullptr);
			assert(n->is_leaf());
			return n;
		}


		void change_usage(int diff) {
			assert(is_leaf());
			assert(diff == 1 || diff == -1); // Nothing else makes sense

			for(Node* p = this; p; p = p->parent) {
				int new_usage = (int)p->usage + diff;
				assert(0 <= new_usage && (size_t)new_usage <= p->data_length);
				p->usage = (size_t)new_usage;
			}
		}

		bool is_nonstrict_parent_of(Node* x) {
			assert(x);
			return this == x || is_parent_of(x);
		}

		bool is_parent_of(Node* x) {
			assert(x);
			return x->m_level < m_level
			    && x->data_index >= data_index
			    && (x->data_index + x->data_length) <= (data_index + data_length);
		}

		Node* first_lawful_parent() {
			for(Node* n = this; n->parent; n = n->parent) {
				n->assert_parenthood();
				Node* p = n->parent;
				if(n != p->left) {
					assert(n == p->right || n == p->buffer);
					return p;
				}
			}
			assert(false); // Fall out
		}

		// Will only return stuff from within itself, asserts nothing else required
		size_t next_usable_strictly_left(size_t i) {
			assert(!is_leaf());
			assert(i > data_index);
			assert(i <= data_index + data_length); // Allow i to be one-outsite my range.

			Node *l = leaf_over(i);
			assert(i >= l->data_index);

			if(i > l->data_index) {
				return i-1;
			} else {
				Node *p = l->first_lawful_parent();

				assert(p);
				assert(!p->is_leaf());
				assert(is_nonstrict_parent_of(p));
				assert(p->right->is_nonstrict_parent_of(l) 
				    || p->buffer->is_nonstrict_parent_of(l));

				Node *ps_left_sibling = p->buffer && p->buffer->is_nonstrict_parent_of(l)
					? p->right 
					: p->left;

				assert(is_parent_of(ps_left_sibling));
				return ps_left_sibling->last_primary();
			}
		}

		size_t n_th_usable(size_t n) {
			assert(n < usable_capacity);
			if (!buffer || n < primary_capacity) {
				return n_th_primary(n);
			}
			return buffer->n_th_primary(n - primary_capacity);
		}

		// Takes a 0-starting ordinal, returns an index
		size_t n_th_primary(size_t n) {
			if(is_leaf()) {
				assert(n < data_length);
				return data_index + n;
			} else {
				assert(left);
				assert(n < primary_capacity);
				if (n < left->primary_capacity) {
					return left->n_th_primary(n);
				} else {
					assert(right);
					assert(n < left->primary_capacity + right->primary_capacity);
					return right->n_th_primary(n - left->primary_capacity);
				}
			}
		}

		size_t last_primary() {
			return n_th_primary(primary_capacity-1);
		}

		string child_string() {
			stringstream ss;
			ss << (left ? "L" : " ")
			   << (right ? "R" : " ")
			   << (buffer ? "B" : " ");
			return ss.str();
		}
		
		string status() {
			stringstream ss;
			ss << child_string()
			   << " lvl: " << m_level 
			   << " lo: " << level_offset 
			   << " usg: " << Usage() 
			   << " prim:" << primary_capacity 
			   << " us_cap:" << usable_capacity 
			   << " tot_cap: " << data_length;
			return ss.str();
		}

		void print_stats() {
			cout << status() << endl;
			if(left) cout << "L " << left->status() << endl;
			if(right) cout << "R " << right->status() << endl;
			if(buffer) cout << "B " << buffer->status() << endl;
		}
	};

	vector<Node*> level_leftmost;
	vector<Node*> level_rightmost;
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
	Sparse_Table(Memory& mem) : m(mem), tree(nullptr, mem, *this) {
		init_tree();
		tree.recalculate_usage();
	};
	void insert_after(size_t index, unsigned value);
	void delete_at(size_t index) {
		m.delete_at(index);
	};
	void print_stats() {
		cout << "tree usage: " << tree.Usage() << endl;
	}

private:
	void clean(Node *x);
	void clean_if_necessary(size_t last_inserted_index);
	size_t first_free_right_of(size_t index);
	void shuffle_right(size_t left_border, size_t right_free);
	size_t next_element_left(size_t i);
};

// Paper discusses maintaining a linked list of occupied elements to speed up this and other traversals.
// This is faster to imlement right now.
// TODO improve maybe
// Note: there must be an element to the left. 
// Note: Nonstrict as per paper.
size_t Sparse_Table::next_element_left(size_t i) {
	i++;
	do { i--;
		if (!m.is_free(i)) return i;
	} while (i != 0);
	assert(false); // Fall out
}

void Sparse_Table::clean(Node *x) {
	assert(!x->is_leaf());
	assert(x->buffer);

	cout << "Doign a cleaning " << endl;
	x->print_stats();

	size_t w = x->n_th_usable(x->Usage());
	do {
		w = x->next_usable_strictly_left(w);
		size_t r = next_element_left(w);
		assert(r >= x->data_index);
		if(r != w) {
			m.write(w, m.read(r));
			m.delete_at(r);
		}
	} while (w != x->data_index);

	tree.recalculate_usage();
	x->print_stats();
	tree.print_stats();
}

void Sparse_Table::insert_after(size_t index, unsigned value) {
	size_t free_spot = first_free_right_of(index);
	if(free_spot != index+1) shuffle_right(index+1, free_spot);
	m.write(index+1, value);
	
	Node *usage_leaf = tree.leaf_over(free_spot);
	assert(usage_leaf);

	usage_leaf->change_usage(1);

	// See if we need cleaning
	// Optionally clean
	Node *hobu; // Highest Overused Buffered Ancestor 
	for(Node *option = tree.leaf_over(index+1); option; option = option->parent) {
		if(!option->buffer) continue;
		if(option->Usage() < option->usable_capacity) continue;
		hobu = option;
	}

	// If tree itself is overused, can't do any cleaning really. 
	if(hobu && hobu != &tree) {
		clean(hobu->parent); 
	}
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
