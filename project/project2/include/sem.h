struct cs1550_sem;
struct pnode;

struct pnode{
    int proc;	//process ID
		struct pnode *next;	//pointer to next process
};

struct cs1550_sem{
    int value;	//value
		struct pnode *front;	//the head of the linked list
		struct pnode *tail;	//the tail of the linked list
};
