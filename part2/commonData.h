#define HT_MAGIC_NO 'P'
#define dump _IOR(HT_MAGIC_NO, 0, int)

struct ht_object{
	int key;
	int data;
};
typedef struct ht_object ht_object_t;

struct dump_arg {
	int n; // the n-th bucket (in) or n objects retrieved (out)
	ht_object_t object_array[8]; // to retrieve at most 8 objects from the n-th bucket
};

#define NAME_LEN 30
struct probe_arg{
	char funcName[NAME_LEN];
	unsigned long funcOffset;			// offset address
	unsigned long localOffset;
	unsigned long globalAddress;
};

typedef struct probe_arg probe_arg_t;
