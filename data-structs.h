
struct long_dllist {
    unsigned int long data;
    unsigned long int count;
    struct long_dllist *prev;
    struct long_dllist *next;
};

struct str_dllist {
    unsigned char *data;
    unsigned long int count;
    struct str_dllist *prev;
    struct str_dllist *next;
};
