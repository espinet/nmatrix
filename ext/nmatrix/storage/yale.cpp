/////////////////////////////////////////////////////////////////////
// = NMatrix
//
// A linear algebra library for scientific computation in Ruby.
// NMatrix is part of SciRuby.
//
// NMatrix was originally inspired by and derived from NArray, by
// Masahiro Tanaka: http://narray.rubyforge.org
//
// == Copyright Information
//
// SciRuby is Copyright (c) 2010 - 2012, Ruby Science Foundation
// NMatrix is Copyright (c) 2012, Ruby Science Foundation
//
// Please see LICENSE.txt for additional copyright notices.
//
// == Contributing
//
// By contributing source code to SciRuby, you agree to be bound by
// our Contributor Agreement:
//
// * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
//
// == yale.c
//
// "new yale" storage format for 2D matrices (like yale, but with
// the diagonal pulled out for O(1) access).
//
// Specifications:
// * dtype and index dtype must necessarily differ
//      * index dtype is defined by whatever unsigned type can store
//        max(rows,cols)
//      * that means vector ija stores only index dtype, but a stores
//        dtype
// * vectors must be able to grow as necessary
//      * maximum size is rows*cols+1

/*
 * Standard Includes
 */

#include <ruby.h>
#include <algorithm>  // std::min
#include <cstdio>     // std::fprintf

/*
 * Project Includes
 */

// #include "types.h"
#include "util/math.h"

#include "data/data.h"

#include "common.h"
#include "yale.h"

#include "nmatrix.h"
#include "ruby_constants.h"

/*
 * Macros
 */
#ifndef NM_MAX
#define NM_MAX(a,b) (((a)>(b))?(a):(b))
#define NM_MIN(a,b) (((a)<(b))?(a):(b))
#endif


/*
 * Global Variables
 */

/*
 * Forward Declarations
 */

extern "C" {
  static YALE_STORAGE*  _copy_alloc_struct(const YALE_STORAGE* rhs, const dtype_t new_dtype, const size_t new_capacity, const size_t new_size);
  static YALE_STORAGE*	alloc(dtype_t dtype, size_t* shape, size_t dim);

  /* Ruby-accessible functions */
  static VALUE nm_size(VALUE self);
  static VALUE nm_a(VALUE self);
  static VALUE nm_d(VALUE self);
  static VALUE nm_lu(VALUE self);
  static VALUE nm_ia(VALUE self);
  static VALUE nm_ja(VALUE self);
  static VALUE nm_ija(VALUE self);

} // end extern "C" block



namespace nm { namespace yale_storage {

template <typename DType, typename IType>
static bool						ndrow_is_empty(const YALE_STORAGE* s, IType ija, const IType ija_next);

template <typename LDType, typename RDType, typename IType>
static bool						ndrow_eqeq_ndrow(const YALE_STORAGE* l, const YALE_STORAGE* r, IType l_ija, const IType l_ija_next, IType r_ija, const IType r_ija_next);

template <typename LDType, typename RDType, typename IType>
static bool           eqeq(const YALE_STORAGE* left, const YALE_STORAGE* right);

template <typename IType>
static YALE_STORAGE*	copy_alloc_struct(const YALE_STORAGE* rhs, const dtype_t new_dtype, const size_t new_capacity, const size_t new_size);

template <typename IType>
static void						increment_ia_after(YALE_STORAGE* s, IType ija_size, IType i, IType n);

template <typename IType>
static IType				  insert_search(YALE_STORAGE* s, IType left, IType right, IType key, bool* found);

template <typename IType>
static inline size_t  get_size(const YALE_STORAGE* storage);

template <typename DType, typename IType>
static char           vector_insert(YALE_STORAGE* s, size_t pos, size_t* j, DType* val, size_t n, bool struct_only);

template <typename DType, typename IType>
static char           vector_insert_resize(YALE_STORAGE* s, size_t current_size, size_t pos, size_t* j, size_t n, bool struct_only);

/*
 * Functions
 */

/*
 * Create Yale storage from IA, JA, and A vectors given in Old Yale format (probably from a file, since NMatrix only uses
 * new Yale for its storage).
 *
 * This function is needed for Matlab .MAT v5 IO.
 */
template <typename LDType, typename RDType, typename IType>
YALE_STORAGE* create_from_old_yale(dtype_t dtype, size_t* shape, void* r_ia, void* r_ja, void* r_a) {
  IType*  ir = reinterpret_cast<IType*>(r_ia);
  IType*  jr = reinterpret_cast<IType*>(r_ja);
  RDType* ar = reinterpret_cast<RDType*>(r_a);

  // Read through ia and ja and figure out the ndnz (non-diagonal non-zeros) count.
  size_t ndnz = 0;
  IType i;
  for (i = 0; i < shape[0]; ++i) { // Walk down rows
    for (IType p = ir[i], p_next = ir[i+1]; p < p_next; ++p) { // Now walk through columns
      if (i != jr[p]) {
      	// entry is non-diagonal and probably nonzero
      	++ndnz;
      }
    }
  }

  // Having walked through the matrix, we now go about allocating the space for it.
  YALE_STORAGE* s = alloc(dtype, shape, 2);
  s->capacity = shape[0] + ndnz + 1;
  s->ndnz     = ndnz;

  // Setup IJA and A arrays
  s->ija = ALLOC_N( IType, s->capacity );
  s->a   = ALLOC_N( LDType, s->capacity );
  IType* ijl    = reinterpret_cast<IType*>(s->ija);
  LDType* al    = reinterpret_cast<LDType*>(s->a);

  // Figure out where to start writing JA in IJA:
  IType pp = s->shape[0]+1;

  // Now fill the arrays
  for (i = 0; i < s->shape[0]; ++i) {

    // Now walk through columns
    for (IType p = ir[i], p_next = ir[i+1]; p < p_next; ++p, ++pp) {

      if (i == jr[p]) { // diagonal
        al[i] = ar[p];
        --pp;

      } else {          // nondiagonal
        ijl[pp] = jr[p];
        al[pp]  = ar[p];
      }
    }
  }

  ijl[i] = pp; // Set the end of the last row

  // Set the zero position for our output matrix
  al[i] = 0;

  return s;
}


/*
 * Take two Yale storages and merge them into a new Yale storage.
 *
 * Uses the left as a template for the creation of a new one.
 */
template <typename DType, typename IType>
YALE_STORAGE* create_merged(const YALE_STORAGE* left, const YALE_STORAGE* right) {
  char ins_type;

  size_t size = get_size<IType>(left);

  // s represents the resulting storage
  YALE_STORAGE* s = copy_alloc_struct<IType>(left, left->dtype, NM_MAX(left->capacity, right->capacity), size);

  IType* sija = reinterpret_cast<IType*>(s->ija);
  IType* rija = reinterpret_cast<IType*>(right->ija);

  // set the element between D and LU (the boundary in A), which should be 0.
  reinterpret_cast<DType*>(s->a)[s->shape[0]] = reinterpret_cast<DType*>(left->a)[left->shape[0]];

  if (right && right != left) {
  	// some operations are unary and don't need this; others are x+x and don't need this

  	for (IType i = 0; i < s->shape[0]; ++i) {

      IType ija       = sija[i];
      IType ija_next  = sija[i+1];

      for (IType r_ija = rija[i]; r_ija < rija[i+1]; ++r_ija) {

        size_t ja = sija[ija]; // insert expects a size_t

        if (ija == ija_next) {
        	// destination row is empty
          ins_type = vector_insert<DType,IType>(s, ija, &ja, NULL, 1, true);
          increment_ia_after<IType>(s, s->shape[0], i, 1);
          ++(s->ndnz);
          ++ija;

          if (ins_type == 'i') ++ija_next;

        } else {
          bool found;

        	// merge positions into destination row
          IType pos = insert_search<IType>(s, ija, ija_next-1, sija[ija], &found);

          if (!found) {
            vector_insert<DType,IType>(s, pos, &ja, NULL, 1, true);
            increment_ia_after<IType>(s, s->shape[0], i, 1);
            ++(s->ndnz);

            if (ins_type == 'i') ++ija_next;
          }

          // can now set a left boundary for the next search
          ija = pos + 1;
        }
      }
    }
  }

  return s;
}


/*
 * Empty the matrix by initializing the IJA vector and setting the diagonal to 0.
 *
 * Called when most YALE_STORAGE objects are created.
 */
template <typename DType, typename IType>
void init(YALE_STORAGE* s) {
  IType IA_INIT = s->shape[0] + 1;

  IType* ija = reinterpret_cast<IType*>(s->ija);
  // clear out IJA vector
  for (IType i = 0; i < IA_INIT; ++i) {
    ija[i] = IA_INIT; // set initial values for IJA
  }

  clear_diagonal_and_zero<DType>(s);
}


///////////////
// Accessors //
///////////////


/*
 * Returns a pointer to the correct location in the A vector of a YALE_STORAGE object, given some set of coordinates
 * (the coordinates are stored in slice).
 */
template <typename DType,typename IType>
void* ref(YALE_STORAGE* storage, SLICE* slice) {
  size_t* coords = slice->coords;

  DType* a = reinterpret_cast<DType*>(storage->a);
  IType* ija = reinterpret_cast<IType*>(storage->ija);

  if (coords[0] == coords[1])
    return &(a[ coords[0] ]); // return diagonal entry

  if (ija[coords[0]] == ija[coords[0]+1])
    return &(a[ storage->shape[0] ]); // return zero pointer

	// binary search for the column's location
  int pos = binary_search<IType>(storage,
                                          ija[coords[0]],
                                          ija[coords[0]+1]-1,
                                          coords[1]);

  if (pos != -1 && ija[pos] == coords[1])
    return &(a[pos]); // found exact value

  return &(a[ storage->shape[0] ]); // return a pointer that happens to be zero
}





/*
 * Attempt to set some cell in a YALE_STORAGE object. Must supply coordinates and a pointer to a value (which will be
 * copied into the storage object).
 */
template <typename DType, typename IType>
char set(YALE_STORAGE* storage, SLICE* slice, void* value) {
  DType* v = reinterpret_cast<DType*>(value);
  size_t* coords = slice->coords;

  bool found = false;
  char ins_type;

  if (coords[0] == coords[1]) {
    reinterpret_cast<DType*>(storage->a)[coords[0]] = *v; // set diagonal
    return 'r';
  }

  // Get IJA positions of the beginning and end of the row
  if (reinterpret_cast<IType*>(storage->ija)[coords[0]] == reinterpret_cast<IType*>(storage->ija)[coords[0]+1]) {
  	// empty row
    ins_type = vector_insert<DType,IType>(storage, reinterpret_cast<IType*>(storage->ija)[coords[0]], &(coords[1]), v, 1, false);
    increment_ia_after<IType>(storage, storage->shape[0], coords[0], 1);
    storage->ndnz++;

    return ins_type;
  }

  // non-empty row. search for coords[1] in the IJA array, between ija and ija_next
  // (including ija, not including ija_next)
  //ija_size = get_size<IType>(storage);

  // Do a binary search for the column
  size_t pos = insert_search<IType>(storage,
                                    reinterpret_cast<IType*>(storage->ija)[coords[0]],
                                    reinterpret_cast<IType*>(storage->ija)[coords[0]+1]-1,
                                    coords[1], &found);

  if (found) { // replace
    reinterpret_cast<IType*>(storage->ija)[pos] = coords[1];
    reinterpret_cast<DType*>(storage->a)[pos]   = *v;
  	return 'r';
  }

  ins_type = vector_insert<DType,IType>(storage, pos, &(coords[1]), v, 1, false);
  increment_ia_after<IType>(storage, storage->shape[0], coords[0], 1);
  storage->ndnz++;

  return ins_type;
}


///////////
// Tests //
///////////


/*
 * Yale eql? -- for whole-matrix comparison returning a single value.
 */
template <typename LDType, typename RDType, typename IType>
static bool eqeq(const YALE_STORAGE* left, const YALE_STORAGE* right) {
  LDType* la = reinterpret_cast<LDType*>(left->a);
  RDType* ra = reinterpret_cast<RDType*>(right->a);

  // Compare the diagonals first.
  for (size_t index = 0; index < left->shape[0]; ++index) {
    if (la[index] != ra[index]) return false;
  }

  IType* lij = reinterpret_cast<IType*>(left->ija);
  IType* rij = reinterpret_cast<IType*>(right->ija);

  for (IType i = 0; i < left->shape[0]; ++i) {

  // Get start and end positions of row
    IType l_ija = lij[i],
          l_ija_next = lij[i+1],
          r_ija = rij[i],
          r_ija_next = rij[i+1];

    // Check to see if one row is empty and the other isn't.
    if (ndrow_is_empty<LDType,IType>(left, l_ija, l_ija_next)) {
      if (!ndrow_is_empty<RDType,IType>(right, r_ija, r_ija_next)) {
      	return false;
      }

    } else if (ndrow_is_empty<RDType,IType>(right, r_ija, r_ija_next)) {
    	// one is empty but the other isn't
      return false;

    } else if (!ndrow_eqeq_ndrow<LDType,RDType,IType>(left, right, l_ija, l_ija_next, r_ija, r_ija_next)) {
    	// Neither row is empty. Must compare the rows directly.
      return false;
    }

  }

  return true;
}


/*
 * Are two non-diagonal rows the same? We already know.
 *
 * FIXME: Add templating.
 */
template <typename LDType, typename RDType, typename IType>
static bool ndrow_eqeq_ndrow(const YALE_STORAGE* l, const YALE_STORAGE* r, IType l_ija, const IType l_ija_next, IType r_ija, const IType r_ija_next) {
  bool l_no_more = false, r_no_more = false;

  IType *lij = reinterpret_cast<IType*>(l->ija),
        *rij = reinterpret_cast<IType*>(r->ija);

  LDType* la = reinterpret_cast<LDType*>(l->a);
  RDType* ra = reinterpret_cast<RDType*>(r->a);

  IType l_ja = lij[l_ija],
        r_ja = rij[r_ija];
        
  IType ja = std::min(l_ja, r_ja);

  while (!(l_no_more && r_no_more)) {
    if (l_ja == r_ja) {

      if (ra[r_ija] != la[l_ija]) return false; // Direct comparison

      ++l_ija;
      ++r_ija;

      if (l_ija < l_ija_next) {
      	l_ja = lij[l_ija];

      } else {
      	l_no_more = true;
      }

      if (r_ija < r_ija_next) {
      	r_ja = rij[r_ija];

      } else {
      	r_no_more = true;
      }

      ja = std::min(l_ja, r_ja);

    } else if (l_no_more || ja < l_ja) {

      if (ra[r_ija] != 0) return false;

      ++r_ija;
      if (r_ija < r_ija_next) {
      	// get next column
      	r_ja = rij[r_ija];
        ja = std::min(l_ja, r_ja);

      } else {
      	l_no_more = true;
      }

    } else if (r_no_more || ja < r_ja) {

      if (la[l_ija] != 0) return false;

      ++l_ija;
      if (l_ija < l_ija_next) {
      	// get next column
        l_ja = lij[l_ija];
        ja = std::min(l_ja, r_ja);
      } else {
      	l_no_more = true;
      }

    } else {
      std::fprintf(stderr, "Unhandled in eqeq: l_ja=%d, r_ja=%d\n", (int)l_ja, (int)r_ja);
    }
  }

	// every item matched
  return true;
}

/*
 * Is the non-diagonal portion of the row empty?
 */
template <typename DType, typename IType>
static bool ndrow_is_empty(const YALE_STORAGE* s, IType ija, const IType ija_next) {
  if (ija == ija_next) return true;

  DType* a = reinterpret_cast<DType*>(s->a);

	// do all the entries = zero?
  for (; ija < ija_next; ++ija) {
    if (a[ija] != 0) return false;
  }

  return true;
}

/////////////
// Utility //
/////////////


/*
 * Binary search for returning stored values. Returns a non-negative position, or -1 for not found.
 */
template <typename IType>
int binary_search(YALE_STORAGE* s, IType left, IType right, IType key) {

  if (left > right) return -1;

  IType* ija = reinterpret_cast<IType*>(s->ija);

  IType mid = (left + right)/2;
  IType mid_j = ija[mid];

  if (mid_j == key)
  	return mid;

  else if (mid_j > key)
  	return binary_search<IType>(s, left, mid - 1, key);

  else
  	return binary_search<IType>(s, mid + 1, right, key);
}



/*
 * Resize yale storage vectors A and IJA in preparation for an insertion.
 */
template <typename DType, typename IType>
static char vector_insert_resize(YALE_STORAGE* s, size_t current_size, size_t pos, size_t* j, size_t n, bool struct_only) {
  // Determine the new capacity for the IJA and A vectors.
  size_t new_capacity = s->capacity * GROWTH_CONSTANT;

  if (new_capacity > NM_YALE_MAX_SIZE(s)) {
    new_capacity = NM_YALE_MAX_SIZE(s);

    if (current_size + n > NM_YALE_MAX_SIZE(s)) rb_raise(rb_eNoMemError, "insertion size exceeded maximum yale matrix size");
  }

  if (new_capacity < current_size + n)
  	new_capacity = current_size + n;

  // Allocate the new vectors.
  IType* new_ija     = ALLOC_N( IType, new_capacity );
  NM_CHECK_ALLOC(new_ija);

  DType* new_a       = ALLOC_N( DType, new_capacity );
  NM_CHECK_ALLOC(new_a);

  IType* old_ija     = reinterpret_cast<IType*>(s->ija);
  DType* old_a       = reinterpret_cast<DType*>(s->a);

  // Copy all values prior to the insertion site to the new IJA and new A
  if (struct_only) {
    for (size_t i = 0; i < pos; ++i) {
      new_ija[i] = old_ija[i];
    }
  } else {
    for (size_t i = 0; i < pos; ++i) {
      new_ija[i] = old_ija[i];
      new_a[i]   = old_a[i];
    }
  }


  // Copy all values subsequent to the insertion site to the new IJA and new A, leaving room (size n) for insertion.
  if (struct_only) {
    for (size_t i = pos; i < current_size - pos + n - 1; ++i) {
      new_ija[i+n] = old_ija[i];
    }
  } else {
    for (size_t i = pos; i < current_size - pos + n - 1; ++i) {
      new_ija[i+n] = old_ija[i];
      new_a[i+n] = old_a[i];
    }
  }

  s->capacity = new_capacity;

  free(s->ija);
  free(s->a);

  s->ija = reinterpret_cast<void*>(new_ija);
  s->a   = reinterpret_cast<void*>(new_a);

  return 'i';
}

/*
 * Insert a value or contiguous values in the ija and a vectors (after ja and
 * diag). Does not free anything; you are responsible!
 *
 * TODO: Improve this so it can handle non-contiguous element insertions
 *	efficiently. For now, we can just sort the elements in the row in
 *	question.)
 */
template <typename DType, typename IType>
static char vector_insert(YALE_STORAGE* s, size_t pos, size_t* j, DType* val, size_t n, bool struct_only) {
  if (pos < s->shape[0]) {
    rb_raise(rb_eArgError, "vector insert pos is before beginning of ja; this should not happen");
  }

  size_t size = get_size<IType>(s);

  IType* ija = reinterpret_cast<IType*>(s->ija);
  DType* a   = reinterpret_cast<DType*>(s->a);

  if (size + n > s->capacity) {
  	vector_insert_resize<DType,IType>(s, size, pos, j, n, struct_only);

    // Need to get the new locations for ija and a.
  	ija = reinterpret_cast<IType*>(s->ija);
    a   = reinterpret_cast<DType*>(s->a);

  } else {
    /*
     * No resize required:
     * easy (but somewhat slow), just copy elements to the tail, starting at
     * the end, one element at a time.
     *
     * TODO: This can be made slightly more efficient, but only after the tests
     *	are written.
     */

    if (struct_only) {
      for (size_t i = 0; i < size - pos; ++i) {
        ija[size+n-1-i] = ija[size-1-i];
      }
    } else {
      for (size_t i = 0; i < size - pos; ++i) {
        ija[size+n-1-i] = ija[size-1-i];
        a[size+n-1-i]   = a[size-1-i];
      }
    }
  }

  // Now insert the new values.
  if (struct_only) {
    for (size_t i = 0; i < n; ++i) {
      ija[pos+i]  = j[i];
    }
  } else {
    for (size_t i = 0; i < n; ++i) {
      ija[pos+i]  = j[i];
      a[pos+i]    = val[i];
    }
  }

  return 'i';
}

/*
 * If we add n items to row i, we need to increment ija[i+1] and onward.
 */
template <typename IType>
static void increment_ia_after(YALE_STORAGE* s, IType ija_size, IType i, IType n) {
  IType* ija = reinterpret_cast<IType*>(s->ija);

  ++i;
  for (; i <= ija_size; ++i) {
    ija[i] += n;
  }
}

/*
 * Binary search for returning insertion points.
 */
template <typename IType>
static IType insert_search(YALE_STORAGE* s, IType left, IType right, IType key, bool* found) {

  if (left > right) {
    *found = false;
    return left;
  }

  IType* ija = reinterpret_cast<IType*>(s->ija);
  IType mid = (left + right)/2;
  IType mid_j = ija[mid];

  if (mid_j == key) {
    *found = true;
    return mid;

  } else if (mid_j > key) {
  	return insert_search<IType>(s, left, mid-1, key, found);

  } else {
  	return insert_search<IType>(s, mid+1, right, key, found);
  }
}

/////////////////////////
// Copying and Casting //
/////////////////////////


/*
 * Templated copy constructor for changing dtypes.
 */
template <typename LDType, typename RDType, typename IType>
YALE_STORAGE* cast_copy(const YALE_STORAGE* rhs, dtype_t new_dtype) {

  // Allocate a new structure
  size_t size = get_size<IType>(rhs);
  YALE_STORAGE* lhs = copy_alloc_struct<IType>(rhs, new_dtype, rhs->capacity, size);

  if (rhs->dtype == new_dtype) {  // FIXME: Test if this condition is actually faster; second condition should work just as well.

    memcpy(lhs->a, rhs->a, size * DTYPE_SIZES[new_dtype]);

  } else {

    LDType* la = reinterpret_cast<LDType*>(lhs->a);
    RDType* ra = reinterpret_cast<RDType*>(rhs->a);

    for (size_t index = 0; index < size; ++index) {
      la[index] = ra[index];
    }

  }

  return lhs;
}



/*
 * Template access for getting the size of Yale storage.
 */
template <typename IType>
static inline size_t get_size(const YALE_STORAGE* storage) {
  return static_cast<size_t>(reinterpret_cast<IType*>(storage->ija)[ storage->shape[0] ]);
}


/*
 * Allocate for a copy or copy-cast operation, and copy the IJA portion of the
 * matrix (the structure).
 */
template <typename IType>
static YALE_STORAGE* copy_alloc_struct(const YALE_STORAGE* rhs, const dtype_t new_dtype, const size_t new_capacity, const size_t new_size) {
  YALE_STORAGE* lhs = ALLOC( YALE_STORAGE );
  lhs->dim          = rhs->dim;
  lhs->shape        = ALLOC_N( size_t, lhs->dim );
  memcpy(lhs->shape, rhs->shape, lhs->dim * sizeof(size_t));
  lhs->itype        = rhs->itype;
  lhs->capacity     = new_capacity;
  lhs->dtype        = new_dtype;
  lhs->ndnz         = rhs->ndnz;

  lhs->ija          = ALLOC_N( IType, lhs->capacity );
  lhs->a            = ALLOC_N( char, DTYPE_SIZES[new_dtype] * lhs->capacity );

  // Now copy the contents -- but only within the boundaries set by the size. Leave
  // the rest uninitialized.
  for (size_t i = 0; i < get_size<IType>(rhs); ++i)
    reinterpret_cast<IType*>(lhs->ija)[i] = reinterpret_cast<IType*>(rhs->ija)[i]; // copy indices

  return lhs;
}


template <typename DType, typename IType>
static STORAGE* matrix_multiply(const STORAGE_PAIR& casted_storage, size_t* resulting_shape, bool vector) {
  YALE_STORAGE *left  = (YALE_STORAGE*)(casted_storage.left),
               *right = (YALE_STORAGE*)(casted_storage.right);

  // We can safely get dtype from the casted matrices; post-condition of binary_storage_cast_alloc is that dtype is the
  // same for left and right.
  // int8_t dtype = left->dtype;

  // Create result storage.
  YALE_STORAGE* result = nm_yale_storage_create(left->dtype, resulting_shape, 2, left->capacity + right->capacity);
  init<DType,IType>(result);

  IType* ijl = reinterpret_cast<IType*>(left->ija);
  IType* ijr = reinterpret_cast<IType*>(right->ija);
  IType* ija = reinterpret_cast<IType*>(result->ija);

  // Symbolic multiplication step (build the structure)
  nm::math::symbmm<IType>(result->shape[0], result->shape[1], ijl, ijl, true, ijr, ijr, true, ija, true);

  // Numeric multiplication step (fill in the elements)
  nm::math::numbmm<DType,IType>(result->shape[0], result->shape[1],
                                ijl, ijl, reinterpret_cast<DType*>(left->a), true,
                                ijr, ijr, reinterpret_cast<DType*>(right->a), true,
                                ija, ija, reinterpret_cast<DType*>(result->a), true);

  // Sort the columns
  nm::math::smmp_sort_columns<DType,IType>(result->shape[0], ija, ija, reinterpret_cast<DType*>(result->a));

  return reinterpret_cast<STORAGE*>(result);
}

}} // end of namespace nm::yale_storage.

///////////////////
// Ruby Bindings //
///////////////////

/* These bindings are mostly only for debugging Yale. They are called from Init_nmatrix. */

extern "C" {

void nm_init_yale_functions() {
  cNMatrix_YaleFunctions = rb_define_module_under(cNMatrix, "YaleFunctions");

  rb_define_method(cNMatrix_YaleFunctions, "yale_ija", (METHOD)nm_ija, 0);
  rb_define_method(cNMatrix_YaleFunctions, "yale_a", (METHOD)nm_a, 0);
  rb_define_method(cNMatrix_YaleFunctions, "yale_size", (METHOD)nm_size, 0);
  rb_define_method(cNMatrix_YaleFunctions, "yale_ia", (METHOD)nm_ia, 0);
  rb_define_method(cNMatrix_YaleFunctions, "yale_ja", (METHOD)nm_ja, 0);
  rb_define_method(cNMatrix_YaleFunctions, "yale_d", (METHOD)nm_d, 0);
  rb_define_method(cNMatrix_YaleFunctions, "yale_lu", (METHOD)nm_lu, 0);
  rb_define_const(cNMatrix_YaleFunctions, "YALE_GROWTH_CONSTANT", rb_float_new(nm::yale_storage::GROWTH_CONSTANT));
}


/////////////////
// C ACCESSORS //
/////////////////

/*
 * C accessor for inserting some value in a matrix (or replacing an existing cell).
 */
char nm_yale_storage_set(STORAGE* storage, SLICE* slice, void* v) {
  NAMED_LI_DTYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::set, char, YALE_STORAGE* storage, SLICE* slice, void* value);

  YALE_STORAGE* casted_storage = (YALE_STORAGE*)storage;

  return ttable[casted_storage->dtype][casted_storage->itype](casted_storage, slice, v);
}


/*
 * Documentation goes here. FIXME: Aleksey, what is this supposed to do?
 *
 * Slicing-related.
 */
void* nm_yale_storage_get(STORAGE* storage, SLICE* slice) {
  //YALE_STORAGE* s = (YALE_STORAGE*)storage;
  rb_raise(rb_eNotImpError, "This type of yale slicing not supported yet");
}


/*
 * C accessor for yale_storage::ref, which returns a pointer to the correct location in a YALE_STORAGE object
 * for some set of coordinates.
 */
void* nm_yale_storage_ref(STORAGE* storage, SLICE* slice) {
  NAMED_LI_DTYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::ref, void*, YALE_STORAGE* storage, SLICE* slice);

  YALE_STORAGE* casted_storage = (YALE_STORAGE*)storage;
  return ttable[casted_storage->dtype][casted_storage->itype](casted_storage, slice);
}

/*
 * C accessor for determining whether two YALE_STORAGE objects have the same contents.
 *
 * FIXME: Is this for element-wise or whole-matrix equality?
 */
bool nm_yale_storage_eqeq(const STORAGE* left, const STORAGE* right) {
  NAMED_LRI_DTYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::eqeq, bool, const YALE_STORAGE* left, const YALE_STORAGE* right);

  const YALE_STORAGE* casted_left = reinterpret_cast<const YALE_STORAGE*>(left);

  return ttable[casted_left->dtype][right->dtype][casted_left->itype](casted_left, (const YALE_STORAGE*)right);
}

/*
 * Copy constructor for changing dtypes. (C accessor)
 */
STORAGE* nm_yale_storage_cast_copy(const STORAGE* rhs, dtype_t new_dtype) {
  NAMED_LRI_DTYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::cast_copy, YALE_STORAGE*, const YALE_STORAGE* rhs, dtype_t new_dtype);

  const YALE_STORAGE* casted_rhs = reinterpret_cast<const YALE_STORAGE*>(rhs);

  return (STORAGE*)ttable[new_dtype][casted_rhs->dtype][casted_rhs->itype](casted_rhs, new_dtype);
}


/*
 * Returns size of Yale storage as a size_t (no matter what the itype is). (C accessor)
 */
inline size_t nm_yale_storage_get_size(const YALE_STORAGE* storage) {
  NAMED_ITYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::get_size, size_t, const YALE_STORAGE* storage);

  return ttable[storage->itype](storage);
}


/*
 * C accessor for allocating a yale storage object for cast-copying. Copies the IJA vector, does not copy the A vector.
 */
static YALE_STORAGE* _copy_alloc_struct(const YALE_STORAGE* rhs, const dtype_t new_dtype, const size_t new_capacity, const size_t new_size) {
  NAMED_ITYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::copy_alloc_struct, YALE_STORAGE*, const YALE_STORAGE* rhs, const dtype_t new_dtype, const size_t new_capacity, const size_t new_size);

  return ttable[rhs->itype](rhs, new_dtype, new_capacity, new_size);
}



/*
 * Transposing copy constructor.
 */
STORAGE* nm_yale_storage_copy_transposed(const STORAGE* rhs_base) {
  YALE_STORAGE* rhs = (YALE_STORAGE*)rhs_base;

  size_t* shape = ALLOC_N(size_t, 2);
  shape[0] = rhs->shape[1];
  shape[1] = rhs->shape[0];

  size_t size   = nm_yale_storage_get_size(rhs);

  YALE_STORAGE* lhs = nm_yale_storage_create(rhs->dtype, shape, 2, size);
  nm_yale_storage_init(lhs);

  NAMED_LI_DTYPE_TEMPLATE_TABLE(transp, nm::math::transpose_yale, void, const size_t n, const size_t m, const void* ia_, const void* ja_, const void* a_, const bool diaga, void* ib_, void* jb_, void* b_, const bool move);

  transp[lhs->dtype][lhs->itype](rhs->shape[0], rhs->shape[1], rhs->ija, rhs->ija, rhs->a, true, lhs->ija, lhs->ija, lhs->a, true);

  return (STORAGE*)lhs;
}

/*
 * C accessor for multiplying two YALE_STORAGE matrices, which have already been casted to the same dtype.
 *
 * FIXME: What happens if the two matrices have different itypes?
 */
STORAGE* nm_yale_storage_matrix_multiply(const STORAGE_PAIR& casted_storage, size_t* resulting_shape, bool vector) {
  NAMED_LI_DTYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::matrix_multiply, STORAGE*, const STORAGE_PAIR& casted_storage, size_t* resulting_shape, bool vector);

  YALE_STORAGE* storage_access = (YALE_STORAGE*)(casted_storage.left);

  return ttable[storage_access->dtype][storage_access->itype](casted_storage, resulting_shape, vector);
}


///////////////
// Lifecycle //
///////////////

/*
 * C accessor function for creating a YALE_STORAGE object. Prior to calling this function, you MUST
 * allocate shape (should be size_t * 2) -- don't use use a regular size_t array!
 *
 * For this type, dim must always be 2. The final argument is the initial capacity with which to
 * create the storage.
 */

YALE_STORAGE* nm_yale_storage_create(dtype_t dtype, size_t* shape, size_t dim, size_t init_capacity) {
  YALE_STORAGE* s;
  size_t max_capacity;

	// FIXME: This error should be handled in the nmatrix.c file.
  if (dim != 2) {
   	rb_raise(rb_eNotImpError, "Can only support 2D matrices");
  }

  s = alloc(dtype, shape, dim);
  max_capacity = nm_storage_count_max_elements(s) - s->shape[0] + 1;

  // Set matrix capacity (and ensure its validity)
  if (init_capacity < NM_YALE_MINIMUM(s)) {
  	s->capacity = NM_YALE_MINIMUM(s);

  } else if (init_capacity > max_capacity) {
  	// Don't allow storage to be created larger than necessary
  	s->capacity = max_capacity;

  } else {
  	s->capacity = init_capacity;
  }

  s->ija = ALLOC_N( char, ITYPE_SIZES[s->itype] * s->capacity );
  s->a   = ALLOC_N( char, DTYPE_SIZES[s->dtype] * s->capacity );

  return s;
}


/*
 * Destructor for yale storage (C-accessible).
 */
void nm_yale_storage_delete(STORAGE* s) {
  if (s) {
    YALE_STORAGE* storage = (YALE_STORAGE*)s;
    free(storage->shape);
    free(storage->ija);
    free(storage->a);
    free(storage);
  }
}


/*
 * C accessor for yale_storage::init, a templated function.
 *
 * Initializes the IJA vector of the YALE_STORAGE matrix.
 */
void nm_yale_storage_init(YALE_STORAGE* s) {
  NAMED_LI_DTYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::init, void, YALE_STORAGE* s);

  ttable[s->dtype][s->itype](s);
}

/*
 * Ruby GC mark function for YALE_STORAGE. C accessible.
 */
void nm_yale_storage_mark(void* storage_base) {
  YALE_STORAGE* storage = (YALE_STORAGE*)storage_base;
  size_t i;

  if (storage && storage->dtype == RUBYOBJ) {
  	for (i = storage->capacity; i-- > 0;) {
      rb_gc_mark(*((VALUE*)((char*)(storage->a) + i*DTYPE_SIZES[RUBYOBJ])));
    }
  }
}


/*
 * Allocates and initializes the basic struct (but not the IJA or A vectors).
 */
static YALE_STORAGE* alloc(dtype_t dtype, size_t* shape, size_t dim) {
  YALE_STORAGE* s;

  s = ALLOC( YALE_STORAGE );

  s->ndnz        = 0;
  s->dtype       = dtype;
  s->shape       = shape;
  s->dim         = dim;
  s->itype       = nm_yale_storage_itype_by_shape(shape);

  return s;
}


YALE_STORAGE* nm_yale_storage_create_from_old_yale(dtype_t dtype, size_t* shape, void* ia, void* ja, void* a, dtype_t from_dtype) {

  NAMED_LRI_DTYPE_TEMPLATE_TABLE(ttable, nm::yale_storage::create_from_old_yale, YALE_STORAGE*, dtype_t dtype, size_t* shape, void* r_ia, void* r_ja, void* r_a);

  // With C++ templates, we don't want to have a 4-parameter template. That would be LDType, RDType, LIType, RIType.
  // We can prevent that by copying ia and ja into the correct itype (if necessary) before passing them to the yale
  // copy constructor.
  itype_t to_itype = nm_yale_storage_itype_by_shape(shape);

  return ttable[dtype][from_dtype][to_itype](dtype, shape, ia, ja, a);

}


//////////////////////////////////////////////
// YALE-SPECIFIC FUNCTIONS (RUBY ACCESSORS) //
//////////////////////////////////////////////

/*
 * Get the size of a Yale matrix (the number of elements actually stored).
 *
 * For capacity (the maximum number of elements that can be stored without a resize), use capacity instead.
 */
static VALUE nm_size(VALUE self) {
  YALE_STORAGE* s = (YALE_STORAGE*)NM_STORAGE(self);

  return rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*(s->shape[0]), s->itype).rval;
}


/*
 * Get the A array of a Yale matrix (which stores the diagonal and the LU portions of the matrix).
 */
static VALUE nm_a(VALUE self) {
  YALE_STORAGE* s = NM_STORAGE_YALE(self);

  size_t size = nm_yale_storage_get_size(s);
  VALUE* vals = ALLOCA_N(VALUE, size);

  for (size_t i = 0; i < size; ++i) {
    vals[i] = rubyobj_from_cval((char*)(s->a) + DTYPE_SIZES[s->dtype]*i, s->dtype).rval;
  }
  VALUE ary = rb_ary_new4(size, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}


/*
 * Get the diagonal ("D") portion of the A array of a Yale matrix.
 */
static VALUE nm_d(VALUE self) {
  YALE_STORAGE* s = NM_STORAGE_YALE(self);

  VALUE* vals = ALLOCA_N(VALUE, s->shape[0]);

  for (size_t i = 0; i < s->shape[0]; ++i) {
    vals[i] = rubyobj_from_cval((char*)(s->a) + DTYPE_SIZES[s->dtype]*i, s->dtype).rval;
  }
  return rb_ary_new4(s->shape[0], vals);
}


/*
 * Get the non-diagonal ("LU") portion of the A array of a Yale matrix.
 */
static VALUE nm_lu(VALUE self) {
  YALE_STORAGE* s = NM_STORAGE_YALE(self);

  size_t size = nm_yale_storage_get_size(s);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < size - s->shape[0] - 1; ++i) {
    vals[i] = rubyobj_from_cval((char*)(s->a) + DTYPE_SIZES[s->dtype]*(s->shape[0] + 1 + i), s->dtype).rval;
  }

  VALUE ary = rb_ary_new4(size - s->shape[0] - 1, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}


/*
 * Get the IA portion of the IJA array of a Yale matrix. This gives the start and end positions of rows in the
 * JA and LU portions of the IJA and A arrays, respectively.
 */
static VALUE nm_ia(VALUE self) {
  YALE_STORAGE* s = NM_STORAGE_YALE(self);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < s->shape[0] + 1; ++i) {
    vals[i] = rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*i, s->itype).rval;
  }

  return rb_ary_new4(s->shape[0]+1, vals);
}


/*
 * Get the JA portion of the IJA array of a Yale matrix. This gives the column indices for entries in corresponding
 * positions in the LU portion of the A array.
 */
static VALUE nm_ja(VALUE self) {
  YALE_STORAGE* s = NM_STORAGE_YALE(self);

  size_t size = nm_yale_storage_get_size(s);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < size - s->shape[0] - 1; ++i) {
    vals[i] = rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*(s->shape[0] + 1 + i), s->itype).rval;
  }

  VALUE ary = rb_ary_new4(size - s->shape[0] - 1, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}


/*
 * Get the IJA array of a Yale matrix.
 */
static VALUE nm_ija(VALUE self) {
  YALE_STORAGE* s = NM_STORAGE_YALE(self);

  size_t size = nm_yale_storage_get_size(s);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < size; ++i) {
    vals[i] = rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*i, s->itype).rval;
  }

 VALUE ary = rb_ary_new4(size, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}

} // end of extern "C" block
