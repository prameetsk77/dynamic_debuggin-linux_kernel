
struct buff_obj{
	pid_t tid;
	int l_value;
	int g_value;
	unsigned long kp_addr;
	uint64_t tsc;
};

typedef struct buff_obj buff_obj_t;