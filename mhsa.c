#include<stdlib.h>
#include<stdio.h>
#include<time.h>
#include<sys/time.h>

#define PARENT(position) (position-1)/2
#define RIGHT_SON(position) (position+1)*2
#define LEFT_SON(position) (position+1)*2-1

typedef struct _node {
    int cid;
    double capacity;
    double load;
    double queuetime; // given by load/capacity
} Node;

void swap(Node *node, int i, int j);
void minheapify(Node *node, int position, int length);
void build_minheap(Node *node, int length);
int has_child(int position, int length);
int node_cmp_capacity(const void *a, const void *b);
void mhsa(int m, int n, double *workload, double *capacity, int *s, double *t);

void swap(Node *node, int i, int j)
{
   Node aux = node[i];
   node[i] = node[j];
   node[j] = aux;
}

void minheapify(Node *node, int position, int length)
{
	int smallst_id = position;
	int right_id = RIGHT_SON(position);
	int left_id = LEFT_SON(position);

	// descobrindo a posicao do menor elemento
	if (right_id < length &&
		node[right_id].queuetime < node[smallst_id].queuetime)
	    smallst_id = right_id;

	if (left_id < length &&
		node[left_id].queuetime < node[smallst_id].queuetime)
	    smallst_id = left_id;

	// swap de posicao (position, smallst_id)
	if (smallst_id != position) {
		// realmente o menor naum eh o pai
		swap(node, position, smallst_id);
		minheapify(node, smallst_id, length);
	}
}

void build_minheap(Node *node, int length)
{
	int elements = length / 2 - 1;

	while (elements >= 0) {
		minheapify(node, elements, length);
		elements--;
	}
}

int has_child(int position, int length)
{
	if (LEFT_SON(position) >= length) {
        // has left child
		return 0;
    }
	else if (RIGHT_SON(position) >= length) {
        // has right child
		return 0;
    }
	else {
        // has not child
        return 1;
    }
}

/* qsort struct comparision function in term of capacity */
int node_cmp_capacity(const void *a, const void *b)
{
    Node *ia = (Node *)a;
    Node *ib = (Node *)b;

    // decrasing order
    return (int)(100.f*ib->capacity - 100.f*ia->capacity);
    /*return (int)(100.f*ia->capacity - 100.f*ib->capacity);
     * float comparison: returns negative if b > a
     *  and positive if a > b. We multiplied result by 100.0
     *      to preserve decimal fraction */

}

void mhsa(int nprocs, int ncomps, double *workload, double *capacity, int *s, double *t)
{
    Node *node;
    register int pid, cid;
    int heapsize;

    struct timeval tint, tend;
    double t1, t2;

    gettimeofday(&tint, NULL);

    heapsize = ncomps;
    node = (Node *) malloc(heapsize * sizeof(Node));

    for (cid = 0; cid < heapsize; cid++) {
        node[cid].capacity = capacity[cid];
        node[cid].load = 0.0;
        node[cid].queuetime = 0.0;
        node[cid].cid = cid;
    }

    qsort(node, heapsize, sizeof(Node), node_cmp_capacity);
    build_minheap(node,heapsize);

    for (pid = 0; pid < nprocs; pid++) {
        cid = node[0].cid;
//        printf("%d\t %d\n",pid,cid);
        node[0].load = node[0].load + workload[pid];
        node[0].queuetime = node[0].load / node[0].capacity;
        //computer_load[cid] = node[0].load;
        minheapify(node,0,heapsize);
//        printf("cid = %d\n", cid);
        s[pid] = cid;
    }

    gettimeofday(&tend, NULL);

    t1 = tint.tv_sec+(tint.tv_usec/1000000.0);
    t2 = tend.tv_sec+(tend.tv_usec/1000000.0);

    *t = t2 - t1;
}

double *get_loads(int nprocs, int ncomps, 
                double *pmips, double *cmips, 
                double **rtt, double **comm,
                int *alloc)
{
//    int comp, i, j;
    double sum;
    double *load;

    load = (double *) malloc(ncomps * sizeof(double));

    for (int comp = 0; comp < ncomps; comp++) {
            sum = 0.0;

            // processing cost 
            for (int i = 0; i < nprocs; i++) {
                    if (alloc[i] == comp) {
                            sum += pmips[i] / cmips[comp];
                    }
            }
            
            // communication cost
            for (int i = 0; i < nprocs; i++) {
                    for (int j = 0; j < nprocs; j++) {
                            if (alloc[i] != alloc[j] && (alloc[i] == comp || alloc[j] == comp)) {
                                    sum += rtt[alloc[i]][alloc[j]] * comm[i][j];
                                    sum += rtt[alloc[j]][alloc[i]] * comm[j][i];
                            }
                    }
            }

	    load[comp] = sum;
    }

    return load;
}

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: ./mhsa input_file\n");
        return EXIT_FAILURE;
    }

    int nprocs, ncomps;
    double *pmips,
           *cmips,
           **rtt,
           **comm;
    int *schedule;
    int sou, des;
    double del, vol;
    double exec_time;
    double *l;

    FILE *fp;

    fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot open file %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    fscanf(fp, "%d", &ncomps);
    fscanf(fp, "%d", &nprocs);
//    printf("%d %d", ncomps, nprocs);

    cmips = (double *) malloc(ncomps * sizeof(double));
    for (int i = 0; i < ncomps; i++) {
        fscanf(fp, "%lf", &cmips[i]);
//        printf("%lf ", cmips[i]);
    }
//    printf("\n");

    pmips = (double *) malloc(nprocs * sizeof(double));
    for (int i = 0; i < nprocs; i++) {
        fscanf(fp, "%lf", &pmips[i]);
//      printf("%lf ", pmips[i]);
    }
//    printf("\n");

    rtt = (double **) malloc(ncomps * sizeof(double *));
    for (int i = 0; i < ncomps; i++) {
        rtt[i] = (double *) malloc(ncomps * sizeof(double));
    }
    for (int i = 0; i < ncomps; i++) {
        for (int j = 0; j < ncomps; j++) {
            fscanf(fp, "%d %d %lf", &sou, &des, &del);
//            printf("%d %d %lf\n", sou, des, del);
            rtt[sou][des] = del;
        }
    }

    comm = (double **) malloc(nprocs * sizeof(double *));
    for (int i = 0; i < nprocs; i++) {
        comm[i] = (double *) malloc(nprocs * sizeof(double));
    }
    for (int i = 0; i < nprocs; i++) {
        for (int j = 0; j < nprocs; j++) {
            fscanf(fp, "%d %d %lf", &sou, &des, &vol);
//            printf("%d %d %lf\n", sou, des, vol);
            comm[sou][des] = vol;
        }
    }

    schedule = (int *) malloc(nprocs * sizeof(int));
    
    mhsa(nprocs, ncomps, pmips, cmips, schedule, &exec_time);


    l = get_loads(nprocs, ncomps, pmips, cmips, rtt, comm, schedule);
    for (int i = 0; i < ncomps; i++) {
        printf("%lf ", l[i]);
    }
    printf("\n");

    return EXIT_SUCCESS;
}
