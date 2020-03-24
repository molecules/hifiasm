#ifndef __HASHTABLE__
#define __HASHTABLE__
#include "khashl.h"
#include "kmer.h"

KHASHL_MAP_INIT(static inline, Count_Table, ha_ct, uint64_t, int, kh_hash_dummy, kh_eq_generic)
KHASHL_MAP_INIT(static inline, Pos_Table, ha_pt, uint64_t, uint64_t, kh_hash_dummy, kh_eq_generic)

#define PREFIX_BITS 16
#define MAX_SUFFIX_BITS 64
#define MODE_VALUE 101

///#define WINDOW 350
///#define THRESHOLD  14

#define WINDOW 375
//#define WINDOW_BOUNDARY 150
#define WINDOW_BOUNDARY 375
///for one side, the first or last WINDOW_UNCORRECT_SINGLE_SIDE_BOUNDARY bases should not be corrected
#define WINDOW_UNCORRECT_SINGLE_SIDE_BOUNDARY 25
#define THRESHOLD  15
#define THRESHOLD_RATE 0.04
#define TAIL_LENGTH int(1/THRESHOLD_RATE)
///#define OVERLAP_THRESHOLD 0.9
#define OVERLAP_THRESHOLD_FILTER 0.9
#define WINDOW_MAX_SIZE WINDOW + TAIL_LENGTH + 3
#define THRESHOLD_MAX_SIZE  31
#define FINAL_OVERLAP_ERROR_RATE 0.03

#define GROUP_SIZE 4
///the max cigar likes 10M10D10M10D10M
///#define CIGAR_MAX_LENGTH THRESHOLD*2+2
#define CIGAR_MAX_LENGTH 31*2+4

typedef struct
{
    volatile int lock;
} Hash_table_spin_lock;

typedef struct
{
	Count_Table** sub_h;
    Hash_table_spin_lock* sub_h_lock;
    int prefix_bits;
    int suffix_bits;
    ///number of subtable
	int size;
    uint64_t suffix_mode;
    uint64_t non_unique_k_mer;
} Total_Count_Table;

typedef struct
{
    uint32_t offset;
    uint32_t readID:31, rev:1;
} k_mer_pos;

typedef struct
{
    k_mer_pos* list;
    uint64_t length;
    uint64_t size;
    uint8_t direction;
    uint64_t end_pos;
} k_mer_pos_list;

typedef struct
{
    k_mer_pos_list* list;
    uint64_t size;
    uint64_t length;
} k_mer_pos_list_alloc;

typedef struct
{
    int C_L[CIGAR_MAX_LENGTH];
    char C_C[CIGAR_MAX_LENGTH];
    int length;
} CIGAR;

typedef struct
{
  ///the begining and end of a window, instead of the whole overlap
  uint64_t x_start;
  uint64_t x_end;
  int y_end;
  int y_start;
  int extra_begin;
  int extra_end;
  int error_threshold;
  int error;
  CIGAR cigar;
} window_list;


typedef struct
{
    window_list* buffer;
    long long length;
    long long size;
} window_list_alloc;

typedef struct
{
    uint64_t* buffer;
    uint64_t length;
    uint64_t size;
} Fake_Cigar;

typedef struct
{
    uint64_t x_id;
    ///the begining and end of the whole overlap
    uint64_t x_pos_s;
    uint64_t x_pos_e;
    uint64_t x_pos_strand;

    uint64_t y_id;
    uint64_t y_pos_s;
    uint64_t y_pos_e;
    uint64_t y_pos_strand;

    uint64_t overlapLen;
    uint64_t shared_seed;
    uint64_t align_length;
    ///uint64_t total_errors;
    uint8_t is_match;
    uint8_t without_large_indel;
    uint64_t non_homopolymer_errors;

    window_list* w_list;
    uint64_t w_list_size;
    uint64_t w_list_length;
    int8_t strong;
    Fake_Cigar f_cigar;

    window_list_alloc boundary_cigars;
} overlap_region;

typedef struct
{
    overlap_region* list;
    uint64_t size;
    uint64_t length;
    ///uint64_t mapped_overlaps_length;
    long long mapped_overlaps_length;
} overlap_region_alloc;

typedef struct
{
	uint64_t opos;
	uint32_t self_offset; // offset on the target read
} k_mer_hit;

static inline uint32_t ha_hit_get_readID(const k_mer_hit *h)
{
	return h->opos >> 33;
}

static inline uint32_t ha_hit_get_rev(const k_mer_hit *h)
{
	return h->opos >> 32 & 1;
}

static inline uint32_t ha_hit_get_offset(const k_mer_hit *h)
{
	return (uint32_t)h->opos;
}

typedef struct
{
    long long* score;
    long long* pre;
    long long* indels;
    long long* self_length;
    long long length;
    long long size;
} Chain_Data;

typedef struct
{
    k_mer_hit* list;
    k_mer_hit* tmp;
    long long length;
    long long size;
    Chain_Data chainDP;
} Candidates_list;

typedef struct
{
	Pos_Table** sub_h;
    Hash_table_spin_lock* sub_h_lock;
    int prefix_bits;
    int suffix_bits;
    ///number of subtable
	int size;
    uint64_t suffix_mode;
    k_mer_pos* pos;
    uint64_t useful_k_mer;
    uint64_t total_occ;
    uint64_t* k_mer_index;
} Total_Pos_Table;

////suffix_bits = 64 in default
inline int recover_hash_code(uint64_t sub_ID, uint64_t sub_key, Hash_code* code,
                             uint64_t suffix_mode, int suffix_bits, int k) // FIXME: not working right now
{
    uint64_t h_key, low_key;
    h_key = low_key = 0;

    low_key = sub_ID << SAFE_SHIFT(suffix_bits);
    low_key = low_key | sub_key;

    h_key = sub_ID >> (64 - suffix_bits);

    code->x[0] = code->x[1] = 0;
    uint64_t mask = ALL >> (64 - k);
    code->x[0] = low_key & mask;

    code->x[1] = h_key << (64 - k);
    code->x[1] = code->x[1] | (low_key >> SAFE_SHIFT(k));

    return 1;
}

static inline int ha_code2rev(const Hash_code *code)
{
	return code->x[1] < code->x[3]? 0 : 1;
}

static inline int ha_get_sub_table_short(uint64_t* get_sub_ID, uint64_t* get_sub_key, uint64_t suffix_mode, int suffix_bits, Hash_code* code, int k)
{ // for k < 32
	int j = ha_code2rev(code);
	uint64_t y = code->x[j<<1|1] << k | code->x[j<<1|0];
	y = yak_hash64(y, (1ULL<<(k+k)) - 1);
	if (y % MODE_VALUE > 3) return 0;
	*get_sub_ID = y >> suffix_bits;
	*get_sub_key = y & suffix_mode;
	return 1;
}

static inline int ha_get_sub_table_long(uint64_t* get_sub_ID, uint64_t* get_sub_key, uint64_t suffix_mode, int suffix_bits, Hash_code* code, int k)
{ // for k > 32
	int j = ha_code2rev(code);
	uint64_t y = code->x[j<<1|1] << k | code->x[j<<1|0];
	y = yak_hash64_64(y);
	if (y % MODE_VALUE > 3) return 0;
	int s = 64 - k;
	uint64_t z = code->x[j<<1|1] >> s ^ y << (s + s) >> (s + s);
	*get_sub_ID = y >> suffix_bits | z << (64 - suffix_bits);
	*get_sub_key = y & suffix_mode;
	return 1;
}

inline int get_sub_table(uint64_t* get_sub_ID, uint64_t* get_sub_key, uint64_t suffix_mode, int suffix_bits, Hash_code* code, int k)
{ // not really working for k<=32
	return ha_get_sub_table_long(get_sub_ID, get_sub_key, suffix_mode, suffix_bits, code, k);
}

inline int insert_Total_Count_Table(Total_Count_Table* TCB, Hash_code* code, int k)
{
    uint64_t sub_ID, sub_key;
    if(!get_sub_table(&sub_ID, &sub_key, TCB->suffix_mode, TCB->suffix_bits, code, k))
    {
        return 0;
    }

    khint_t t;  
    int absent;

    while (__sync_lock_test_and_set(&TCB->sub_h_lock[sub_ID].lock, 1))
    {
        while (TCB->sub_h_lock[sub_ID].lock);
    }

    t = ha_ct_put(TCB->sub_h[sub_ID], sub_key, &absent);
    if (absent)
    {
        kh_val(TCB->sub_h[sub_ID], t) = 1;
    }
    else   
    {
        //kh_value(TCB->sub_h[sub_ID], t) = kh_value(TCB->sub_h[sub_ID], t) + 1;
        kh_val(TCB->sub_h[sub_ID], t)++;
    }

    __sync_lock_release(&TCB->sub_h_lock[sub_ID].lock);

    return 1;
}

inline int get_Total_Count_Table(Total_Count_Table* TCB, Hash_code* code, int k)
{
    uint64_t sub_ID, sub_key;
    if(!get_sub_table(&sub_ID, &sub_key, TCB->suffix_mode, TCB->suffix_bits, code, k))
    {
        return 0;
    }

    khint_t t;  

    t = ha_ct_get(TCB->sub_h[sub_ID], sub_key);

    if (t != kh_end(TCB->sub_h[sub_ID]))
    {
        return kh_val(TCB->sub_h[sub_ID], t);
    }
    else
    {
        return 0;
    }
}

inline uint64_t get_Total_Pos_Table(Total_Pos_Table* PCB, Hash_code* code, int k, uint64_t* r_sub_ID)
{
    uint64_t sub_ID, sub_key;
    if(!get_sub_table(&sub_ID, &sub_key, PCB->suffix_mode, PCB->suffix_bits, code, k))
    {
        return (uint64_t)-1;
    }

    khint_t t;  

    t = ha_pt_get(PCB->sub_h[sub_ID], sub_key);

    if (t != kh_end(PCB->sub_h[sub_ID]))
    {
        *r_sub_ID = sub_ID;
        return kh_val(PCB->sub_h[sub_ID], t);
    }
    else
    {
        return (uint64_t)-1;
    }
}

inline uint64_t count_Total_Pos_Table(Total_Pos_Table* PCB, Hash_code* code, int k)
{
    uint64_t sub_ID;
    uint64_t ret = get_Total_Pos_Table(PCB, code, k, &sub_ID);
    if(ret != (uint64_t)-1)
    {
        return PCB->k_mer_index[ret + 1] - PCB->k_mer_index[ret];
    }
    else
    {
        return 0;
    }
}


inline uint64_t locate_Total_Pos_Table(Total_Pos_Table* PCB, Hash_code* code, k_mer_pos** list, int k, uint64_t* r_sub_ID)
{
    uint64_t ret = get_Total_Pos_Table(PCB, code, k, r_sub_ID);
    if(ret != (uint64_t)-1)
    {
        *list = PCB->k_mer_index[ret] + PCB->pos;
        return PCB->k_mer_index[ret + 1] - PCB->k_mer_index[ret];
    }
    else
    {
        *list = NULL;
        return 0;
    }
}

int cmp_k_mer_pos(const void * a, const void * b);


inline uint64_t insert_Total_Pos_Table(Total_Pos_Table* PCB, Hash_code* code, int k, uint64_t readID, uint64_t pos)
{
    k_mer_pos* list;
    int flag = 0;
    uint64_t sub_ID;
    uint64_t occ = locate_Total_Pos_Table(PCB, code, &list, k, &sub_ID);

    if (occ)
    {   
        while (__sync_lock_test_and_set(&PCB->sub_h_lock[sub_ID].lock, 1))
        {
            while (PCB->sub_h_lock[sub_ID].lock);
        }

        if (list[0].offset + 1 < occ)
        {
            list[0].offset++; // if not the last k-mer, this field is reused to keep the number of inserted positions
            list[list[0].offset].readID = readID;
            list[list[0].offset].offset = pos;
            list[list[0].offset].rev = ha_code2rev(code);
        }
        else // now comes to the last k-mer position; then save it to list[0]
        {
            list[0].readID = readID;
            list[0].offset = pos;
			list[0].rev = ha_code2rev(code);
            flag = 1;
        }

        __sync_lock_release(&PCB->sub_h_lock[sub_ID].lock);

        //if all pos has been saved, it is safe to sort
        if (flag && occ>1)
        {
            qsort(list, occ, sizeof(k_mer_pos), cmp_k_mer_pos);
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

void init_Total_Count_Table(int k, Total_Count_Table* TCB);
void init_Total_Pos_Table(Total_Pos_Table* TCB, Total_Count_Table* pre_TCB);
void destory_Total_Count_Table(Total_Count_Table* TCB);

void init_Count_Table(Count_Table** table);
void init_Pos_Table(Count_Table** pre_table, Pos_Table** table);
void destory_Total_Pos_Table(Total_Pos_Table* TCB);
void write_Total_Pos_Table(Total_Pos_Table* TCB, char* read_file_name);
int load_Total_Pos_Table(Total_Pos_Table* TCB, char* read_file_name);



void Traverse_Counting_Table(Total_Count_Table* TCB, Total_Pos_Table* PCB, int k_mer_min_freq, int k_mer_max_freq);

void init_Candidates_list(Candidates_list* l);
void clear_Candidates_list(Candidates_list* l);
void destory_Candidates_list(Candidates_list* l);


void init_k_mer_pos_list_alloc(k_mer_pos_list_alloc* list);
void destory_k_mer_pos_list_alloc(k_mer_pos_list_alloc* list);
void clear_k_mer_pos_list_alloc(k_mer_pos_list_alloc* list);
void append_k_mer_pos_list_alloc(k_mer_pos_list_alloc* list, k_mer_pos* n_list, uint64_t n_length, 
uint64_t n_end_pos, uint8_t n_direction);



void init_overlap_region_alloc(overlap_region_alloc* list);
void clear_overlap_region_alloc(overlap_region_alloc* list);
void destory_overlap_region_alloc(overlap_region_alloc* list);
void append_window_list(overlap_region* region, uint64_t x_start, uint64_t x_end, int y_start, int y_end, int error,
int extra_begin, int extra_end, int error_threshold);



void overlap_region_sort_y_id(overlap_region *a, long long n);


void calculate_overlap_region_by_chaining(Candidates_list* candidates, overlap_region_alloc* overlap_list, 
uint64_t readID, uint64_t readLength, All_reads* R_INF, double band_width_threshold, int add_beg_end);



void init_fake_cigar(Fake_Cigar* x);
void destory_fake_cigar(Fake_Cigar* x);
void clear_fake_cigar(Fake_Cigar* x);
void add_fake_cigar(Fake_Cigar* x, uint32_t gap_site, int32_t gap_shift);
void resize_fake_cigar(Fake_Cigar* x, uint64_t size);
int get_fake_gap_pos(Fake_Cigar* x, int index);
int get_fake_gap_shift(Fake_Cigar* x, int index);
inline long long y_start_offset(long long x_start, Fake_Cigar* o)
{
    if(x_start == get_fake_gap_pos(o, o->length - 1))
    {
        return get_fake_gap_shift(o, o->length - 1);
    }
    
    
    long long i;
    for (i = 0; i < (long long)o->length; i++)
    {
        if(x_start < get_fake_gap_pos(o, i))
        {
            break;
        }
    }

    if(i == 0 || i == (long long)o->length)
    {
        fprintf(stderr, "ERROR\n");
        exit(0);
    }

    ///note here return i - 1
    return get_fake_gap_shift(o, i - 1);
}

inline void print_fake_gap(Fake_Cigar* o)
{
    long long i;
    for (i = 0; i < (long long)o->length; i++)
    {
        fprintf(stderr, "**i: %lld, gap_pos_in_x: %d, gap_shift: %d\n", 
           i, get_fake_gap_pos(o, i),
           get_fake_gap_shift(o, i));
    }

}

void resize_Chain_Data(Chain_Data* x, long long size);
void init_window_list_alloc(window_list_alloc* x);
void clear_window_list_alloc(window_list_alloc* x);
void destory_window_list_alloc(window_list_alloc* x);
void resize_window_list_alloc(window_list_alloc* x, long long size);


#endif
