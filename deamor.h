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
	size_t alpha = 9;
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


		size_t data_index;
		size_t data_length;
		size_t primary_capacity;
		size_t usable_capacity;
		size_t m_level = 0;
		size_t level_offset = 0;

		bool pending_extra = false;
	private:
		bool usage_fresh = false;
		bool is_cleaning = false;
		size_t write_index = 0;
		size_t read_index = 0;

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

		size_t Usage() { assert(usage_fresh); return usage; };
		bool is_parent() { return left; }
		bool is_leaf() { return !is_parent(); }
		bool get_is_cleaning() {
			return is_cleaning;
		}
		void enable_cleaning(size_t w) {
			assert(is_cleaning == false);

			usage_fresh = false; // TODO PArt of my hacky scheme
			set_w(w);
			is_cleaning = true;
		}
		void disable_cleaning() {
			assert(is_cleaning);
			assert(pending_extra == false);
			assert(st.writers[write_index] == this);

			is_cleaning = false;
			st.writers[write_index] = nullptr;
			write_index = 0;
		}
		size_t get_w() {
			assert(is_cleaning);
			return write_index;
		}
		void set_w(size_t w) {
			assert(index_in_range(w));

			assert(st.writers[w] == nullptr);

			if(is_cleaning) { // It's already active somewhere
				assert(st.writers[write_index] == this);
				st.writers[write_index] = nullptr;
			}
			st.writers[w] = this;
			write_index = w;
		}

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

		void bubble_update_usage() {
			for(Node* n = this; n; n = n->parent) {
				n->update_usage();
			}
		}

		// Make sure my usage matches my children
		void update_usage() {
			if(is_leaf()) {
				recalculate_usage();
				return;
			}

			usage = 0;
			assert(left);
			usage = left->Usage();
			if(!right) { assert(!buffer); return; }

			usage += right->Usage();
			if(!buffer) return;

			usage += buffer->Usage();
		}

		// Recalc my entire subtree
		void recalculate_usage() {
			usage_fresh = true;
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

		// [data_index, writeptr)
		bool in_cleaning_interval(size_t i) {
			return i >= data_index && i < write_index;
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
	vector<Node*> writers;
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

		assert(leaves.size() != 0);
		writers = vector<Node*>(capacity, nullptr);

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
	void clean(Node *);
	void clean_step(Node *);
	void start_cleanup(Node *);
	void continue_cleanup(Node *);

	size_t first_free_right_of(int index);
	bool is_slack(size_t);
	Node* writer_at(size_t);
	void shuffle_right(size_t left_border, size_t right_free);
	size_t next_element_left(size_t i);
};

Sparse_Table::Node* Sparse_Table::writer_at(size_t i) {
	return writers[i]; 
}

bool Sparse_Table::is_slack(size_t i) {
	return i == tree.leaf_over(i)->data_index;
}

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

/*
void Sparse_Table::clean(Node *x) {
	assert(!x->is_leaf());
	assert(x->buffer);

	if(verbose) {
		cout << "Doign a cleaning " << endl;
		x->print_stats();
	}

	x->set_w(x->n_th_usable(x->Usage()))
	do { 
		clean_step(x);
	} while (x->get_w() != x->n_th_usable(0));

	tree.recalculate_usage(); // TODO THis will probbly be different more or less but no idea how exactly
	if(verbose) {
		x->print_stats();
		tree.print_stats();
	}
}
*/

void Sparse_Table::start_cleanup(Node* y) {
	y->enable_cleaning(y->n_th_usable(y->Usage()));
	assert(y->pending_extra == false);
	y->pending_extra = false;
	continue_cleanup(y);


	// TODO  When startup returns, update usage of endpoints of zero gap, and  more...
}

// Ignoring second param from paper, probbly also a mistake
void Sparse_Table::continue_cleanup(Node* y) {
	for(size_t i = 0; i < alpha*L && y->get_is_cleaning(); i++) {
		clean_step(y);
	}
	assert(y->pending_extra == false);
}

void Sparse_Table::clean_step(Node* y) { 
	size_t w = y->pending_extra  // Opposite from paper because I believe that one's a mistake
	         ? y->get_w()-1
	 	 : y->next_usable_strictly_left(y->get_w());

	if(is_slack(w)) {
		y->pending_extra = false;
	}
	size_t r = next_element_left(w);
	assert(r >= y->data_index); // No change for slack here

	if(r != w) {
		m.write(w, m.read(r));
		m.delete_at(r);
	}

	// TODO WHen leaf finished, update usage
	
	Node* x = writer_at(r);
	if(x && x != y) {
		x->disable_cleaning();
	}

	y->set_w(w); // Delayed to keep invariants

	if(y->get_w() == y->data_index + 1) {
		// Now either we did use the slack slot to restore equality,
		// or the NEXT one would have been it in which case it should be occupied, right?
		if(y->pending_extra) {
			assert(!m.is_free(y->data_index));
			// The next noop step would have taken the pending extra so:
			y->pending_extra = false;
		}
		y->disable_cleaning(); 
	}

	//y->read_index = next_element_left(min(y->read_index, y->write_index)); // Small departure from paper but we scan much faster this way
}

void Sparse_Table::insert_after(int index, unsigned value) {
	size_t s2 = first_free_right_of(index);
	size_t i_after = (size_t)(index+1);
	if(s2 != i_after) shuffle_right(i_after, s2);
	m.write(index+1, value);
	
	Node *s2_leaf = tree.leaf_over(s2);
	assert(s2_leaf);

	// Increment usage as per algo
	//s2_leaf->change_usage(1); Don't because it's pointless anyway now

	if(strategy == NOCLEAN) return;

	// See if we need cleaning
	for(Node *x = s2_leaf; x && x->parent; x = x->parent) {
		if(!x->buffer) continue;

		Node *y = x->parent;
		assert(y);
		if(y->get_is_cleaning()) {
			if(s2 >= y->data_index && s2 < y->get_w()) {
				y->pending_extra = true;
				// We just inserted something inside of y's ongoing cleaning
				// Thus violating equality
				// therefore within the cleaning rn, y is allowed to put one
				// thing into a slack slot to restore balance.
			}
			continue_cleanup(y);
		} else {
			// Check for ancestor cleaning
			bool can_start = true;
			for(Node* p = y->parent; p; p = p->parent) {
				if(p->get_is_cleaning()
					&& y->in_cleaning_interval(p->get_w())) {
					can_start = false;
					break;
				}
			}

			// Hacky, ineffieient, but faster to implement and there is already too much insecurity
			//x->recalculate_usage(); // TODO
			y->recalculate_usage(); // Actually
			if(can_start && x->Usage() >= x->cleaning_treshold()) {
				start_cleanup(y);
			}
		}
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
