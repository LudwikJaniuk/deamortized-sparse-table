#include <cassert>
#include <cmath>
#include <string>
#include <sstream>
#include <iostream>
#include <limits>
#include "memory.h"

using namespace std;

class Sparse_Table {
public:
	Memory& m;
	size_t L;
	size_t lgL;
	size_t depth = 0;
	size_t capacity = 0;
	bool verbose = false;
	enum Strategy {
		NOCLEAN, CLEAN
	} strategy;

	struct Node {
		Node* parent = nullptr; // Set before init

		// Level-wise, not always topologically
		Node* l_sibling = nullptr;
		Node* r_sibling = nullptr;

		Node* left = nullptr;
		Node* right = nullptr;
		Node* buffer = nullptr;

		size_t write_index = 0;
		size_t read_index = 0;

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

		size_t cleaning_treshold() {
			// 2^l * (L + 1/2)
			return (1 << m_level)*st.L + (1 << (m_level-1)); // Even with slacks it's still the same treshold
		}

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
				data_length = st.leaf_size(); 
				primary_capacity = st.L; // Slack nochange
				usable_capacity = st.L; // Slack nochange
				st.leaves.push_back(this);
			} else {
				left = new Node(this,  m, st);
				left->init(m_level-1,  index);

				right = new Node(this, m, st);
				right->init(m_level-1, index + left->data_length);

				data_length = left->data_length + right->data_length;
				primary_capacity = left->primary_capacity + right->primary_capacity;
				usable_capacity = primary_capacity;

				if (m_level >= st.lgL) { // Buffered!
					buffer = new Node(this, m, st);
					buffer->init(m_level-st.lgL, index + data_length);
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
			    || (parent->right && parent->buffer && parent->buffer == this && (m_level+st.lgL == parent->m_level)));
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

		bool index_in_range(size_t index) { // No change for slack
			return index >= data_index && index < data_index + data_length;
		}

		Node *leaf_over(size_t index) { // No change for slack
			size_t ls = st.leaf_size();
			assert(st.leaves[0]->data_length == ls);

			size_t leaf_index = index / ls;
			assert(st.leaves.size() > leaf_index);

			Node *l = st.leaves[leaf_index];
			assert(l);

			return l;
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
			assert(i > data_index+1); 
			assert(i <= data_index + data_length); // Allow i to be one-outsite my range.

			Node *l = leaf_over(i);
			assert(i >= l->data_index);

			if(i > l->data_index+1) { 
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
				assert(n < data_length-1);
				return data_index + 1 + n; // Slack +1
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
	vector<Node*> leaves;
	Node tree;

	void init_tree() {
		// Determine capacity
		vector<size_t> level_capacity;
		for(depth = 0; ; depth++) {
			size_t required_mem = 0;
			if (depth == 0) {
				required_mem = leaf_size(); 
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
		cout << "Prim cap:" << tree.primary_capacity << endl;
		cout << "usable cap:" << tree.usable_capacity << endl;
		cout << "DATA LEN: " << tree.data_length << endl;

	}
public:
	Sparse_Table(Memory& mem, size_t p_L, size_t p_lgL) 
		: m(mem)
		, L(p_L)
		, lgL(p_lgL)
		, tree(nullptr, mem, *this)
	{
		init_tree();
		tree.recalculate_usage();
	};
	void insert_after(int index, unsigned value);
	void delete_at(size_t index) {
		m.delete_at(index);
	};
	void print_stats() {
		cout << "tree usage: " << tree.Usage() << endl;
	}

	size_t leaf_size() {
		return L+1;
	}

private:
	void clean(Node *x);
	void clean_step(Node* x);
	size_t first_free_right_of(int index);
	void shuffle_right(size_t left_border, size_t right_free);
	size_t next_element_left(size_t i);
};

// Paper discusses maintaining a linked list of occupied elements to speed up this and other traversals.
// This is faster to imlement right now.
// TODO improve maybe
// Note: there must be an element to the left. 
// Note: Nonstrict as per paper.
size_t Sparse_Table::next_element_left(size_t i) { // No change for slack
	i++;
	do { i--;
		if (!m.is_free(i)) return i;
	} while (i != 0);
	assert(false); // Fall out
}

void Sparse_Table::clean(Node *x) {
	assert(!x->is_leaf());
	assert(x->buffer);

	if(verbose) {
		cout << "Doign a cleaning " << endl;
		x->print_stats();
	}

	x->write_index = x->n_th_usable(x->Usage());
	x->read_index = numeric_limits<size_t>::max();
	do { 
		clean_step(x);
	} while (x->write_index != x->n_th_usable(0));

	tree.recalculate_usage(); // TODO THis will probbly be different more or less but no idea how exactly
	if(verbose) {
		x->print_stats();
		tree.print_stats();
	}
}

void Sparse_Table::clean_step(Node* x) { // TODO ALgo will chnage but not right now with slacks
	x->write_index = x->next_usable_strictly_left(x->write_index);
	x->read_index = next_element_left(min(x->read_index, x->write_index)); // Small departure from paper but we scan much faster this way
	//size_t r = next_element_left(w); 
	assert(x->read_index >= x->data_index); // No change for slack here
	if(x->read_index != x->write_index) {
		m.write(x->write_index, m.read(x->read_index));
		m.delete_at(x->read_index);
	}
}

void Sparse_Table::insert_after(int index, unsigned value) {
	size_t free_spot = first_free_right_of(index);
	size_t i_after = (size_t)(index+1);
	if(free_spot != i_after) shuffle_right(i_after, free_spot);
	m.write(index+1, value);
	
	Node *usage_leaf = tree.leaf_over(free_spot);
	assert(usage_leaf);

	usage_leaf->change_usage(1);

	if(strategy == NOCLEAN) return;

	// See if we need cleaning
	Node *hobu = nullptr; // Highest Overused Buffered Ancestor 
	for(Node *option = usage_leaf; option; option = option->parent) {
		if(!option->buffer) continue;
		if(option->Usage() < option->cleaning_treshold()) continue; // Changing to a lower treshold
		hobu = option;
	}

	// If tree itself is overused, can't do any cleaning really. 
	if(hobu && hobu != &tree) {
		clean(hobu->parent); 
	}
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

size_t Sparse_Table::first_free_right_of(int index) {
	assert(index >= 0 || index == -1);
	for(size_t i = index+1; i < m.data.size(); i++) {
		if (m.is_free(i)) return i;
	}
	assert(false);
}
