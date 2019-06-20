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

		// NOte: usage is either accurate, or if a parent is zero then the value doenst matter and it's zero by definition. 
		// If I'm being overridden, uncrementing is kinda pointless no?
		size_t Usage() { 
			for(Node* a = parent; a; a = a->parent) {
				if(a->usage == 0) return 0;
			}

			return usage; 
		};
		bool is_parent() { return left; }
		bool is_leaf() { return !is_parent(); }
		bool get_is_cleaning() {
			return is_cleaning;
		}
		void enable_cleaning(size_t w) {
			assert(is_cleaning == false);

			set_w(w);
			is_cleaning = true;
		}
		void disable_cleaning() {
			assert(is_cleaning);
			assert(pending_extra == false);
			assert(st.writers[write_index] == this);

			is_cleaning = false;
			st.writers[write_index] = nullptr;
			//write_index = 0; // We will actually be reading the final value later soo...
		}

		size_t get_w() {
			assert(is_cleaning);
			return write_index;
		}

		// Hacky passby
		size_t get_last_w() {
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



		// Set usage to 0 if this whole tre is insize the logic gap made by these two
		void zero_if_subtree(size_t r, size_t w) { 
			assert(r < w);
			if(data_index > r && data_index + data_length - 1 < w) {
				usage = 0;
			}
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

		// If I'm being overridden, incrementing is kinda pointless. 
		// But if itäs all zero because of first time, I do want it to happen
		// its just we might get bullshit
		void change_usage(int diff) {
			assert(is_leaf());
			assert(diff == 1); // Nothing else makes sense

			//bool expecting_override = false;
			//bool found_override = false;

			// If we're being zeroed-out, we ought to make it explicit to make counts and asserts work again
			Node * highest_zero = nullptr;
			for(Node* p = this; p; p = p->parent) {
				if(p->usage == 0) {
					highest_zero = p;
				}
			}
			if(highest_zero) {
				bool valid_run = false;
				for(Node* p = this; p; p = p->parent) {
					if (p == highest_zero) {
						valid_run = true;
						break;
					}

					p->usage = 0;
				}
				assert(valid_run); // And not a fallthrough withough meethign that parent
			}

			for(Node* p = this; p; p = p->parent) {
				int new_usage = (int)p->usage + diff;
				assert(0 <= new_usage);

				// THis assertion might be broken if weäre incrementing on an overridden node
				// // Were counteracting above, hope it works
				assert((size_t)new_usage <= p->data_length);

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
		tree.recalculate_usage(); // Well I guess it can stay here
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

void Sparse_Table::start_cleanup(Node* y) {
	// Are we skipping this one slot? Yes that's the point, we're supposed to skip it
	y->enable_cleaning(y->n_th_usable(y->Usage()));

	assert(y->pending_extra == false);
	y->pending_extra = false;
	continue_cleanup(y);
}

// Ignoring second param from paper, probbly also a mistake
void Sparse_Table::continue_cleanup(Node* y) {
	Node *last_leaf = nullptr;
	for(size_t i = 0; i < alpha*L && y->get_is_cleaning(); i++) {
		clean_step(y);

		if(last_leaf == nullptr) {
			last_leaf = tree.leaf_over(y->get_last_w());
			continue;
		}

		Node* curr_leaf = tree.leaf_over(y->get_last_w());
		if(curr_leaf != last_leaf) {
			assert(last_leaf);
			last_leaf->bubble_update_usage();
			last_leaf = curr_leaf;
		}
	}
	assert(y->pending_extra == false);

	// WHen is a leaf finished? If we just wrote on its leftmost usable slot? Buut we might still use the slack!
	// If we're at the leftmost usable slot of a leaf, we still can't know if the next write will be in teh slack slot.
	// Can we know that the previous write was on a different leaf tho?
	// IMplicitly or explicitly?
	// clean_step ALWAYS moves the write pointer at least one step to the left. Might be much more tho. 
	// Ok well seems like the easiest way is to maintain a "last_leaf" pointer explicitly inside of a 
	// continuecleanup and pass it into the cleanupstep all the time. 
	// OHH but reasonably that would always be the leaf of the last w. Except the first time. 
	// Does it hurt to recompute the "last leaf" even the first time?
	// it woudl be a departure that we can avoid with a boolean.
	// so lets avoid it. 
	// OHHH and actually what we can do is put it in continue
	// TODO WHen leaf finished, update usage
	
	// TODO  When cleanup returns, update usage of endpoints of zero gap, and  more...

	size_t w = y->get_last_w();
	size_t r = next_element_left(w);

	assert(!m.is_free(w));
	assert(!m.is_free(r));

	Node* r_leaf = tree.leaf_over(r);
	Node* w_leaf = tree.leaf_over(w);


	if (r_leaf != w_leaf) { // otherwise No logic gap big enough for any more action
		assert(r != w);

		// r and w are endpoints of the logic gap
		// Frist zero all subtrees inside
		for(Node *a = r_leaf->parent; a; a = a->parent) {
			assert(a->left);
			assert(a->left->data_index <= r); // Left would never be one of the subtrees inside logic gap

			assert(a->right);
			a->right->zero_if_subtree(r, w); 

			if(a->buffer) a->buffer->zero_if_subtree(r, w); 

			if(a == y) break; // OPT just an optimization
		}


		for(Node *a = w_leaf->parent; a; a = a->parent) {
			assert(a->left);
			a->left->zero_if_subtree(r, w); 

			assert(a->right);
			a->right->zero_if_subtree(r, w); 

			// Buffer shold never be an optin
			if(a->buffer) assert(a->buffer->data_index + a->buffer->data_length - 1 >= w); 

			if(a == y) break; // OPT just an optimization
		}

		// THen bubble the sentinels
		r_leaf->bubble_update_usage(); // if they're the same no point doing it twice
	}
	w_leaf->bubble_update_usage();
	// And now y's usage should be making sense
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
	s2_leaf->change_usage(1); 

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
			//y->recalculate_usage(); // Actually
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
