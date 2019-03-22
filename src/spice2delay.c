// spice2delay
//
//
//
//
//
//
//
// Todo
// 2) double check all read in c and r values for units

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hash.h"
#include "readliberty.h"	/* liberty file database */

#define SRC     0x01    // node is a driver
#define SNK     0x02    // node is a receiver
#define INT     0x03    // node is internal to an interconnect

typedef struct _cell_io *cell_io_ptr;

typedef struct _cell_io {
    char        *name;
    pinptr      *pins;
    cell_io_ptr next;
} cell_io;

typedef struct _r *rptr;
typedef struct _node *nodeptr;

typedef struct _r {
    char       *name;
    nodeptr     node1;
    nodeptr     node2;
    double      rval;
} r;

typedef struct _ritem* ritemptr;

typedef struct _ritem {
    rptr        r;
    ritemptr    next;
} ritem;


typedef struct _node {
    char*       name;
    int         type;
    ritemptr    rlist;
    double      nodeCap;
    double      totCapDownstream;
    short       visited;
} node;

typedef struct _node_item *node_item_ptr;

typedef struct _node_item {
    nodeptr         node;
    node_item_ptr   next;
} node_item;

typedef struct _snk* snkptr;

typedef struct _snk {
    nodeptr     snknode;
    double      delay;
    snkptr      next;
} snk;

typedef struct _elmdly* elmdlyptr;

typedef struct _elmdly {
    nodeptr     src;
    snkptr      snklist;
} elmdly;

void print_help () {
}

char** tokenize_line (char *line, const char *delims, char*** tokens_ptr, int *num_toks) {
    int buff_sz = 4;

    char **tokens = calloc(buff_sz, sizeof(char*));

    int i = 0;

    tokens[i] = strtok(line, delims);
    i++;

    for (i = 1; tokens[i-1] != NULL; i++) {
        if (i == buff_sz) {
            buff_sz *= 2;
            tokens = realloc(tokens, sizeof(char*) * buff_sz);
        }
        *num_toks = i;
        tokens[i] = strtok(NULL, delims);
    }

    /**tokens_ptr = tokens;*/
    return tokens;
}

void process_subckt_def(char **tokens, int num_toks, Cell *cells, cell_io_ptr *cell_io_ptrptr) {

    int i;

    Cell *cell;

    cell_io_ptr new_cell_io = calloc(1, sizeof(cell_io));

    new_cell_io->name = calloc((strlen(tokens[1]) + 1), sizeof(char));
    new_cell_io->next = NULL;
    strcpy(new_cell_io->name, tokens[1]);

    new_cell_io->pins = calloc((num_toks - 2), sizeof(Pin*));

    cell = get_cell_by_name(cells, tokens[1]);

    for (i = 2; i < num_toks; i++) {
        new_cell_io->pins[i-2] = get_pin_by_name(cell, tokens[i]);
    }

    if (*cell_io_ptrptr == NULL) {
        *cell_io_ptrptr = new_cell_io;
    } else {
        cell_io_ptr curr_cell_io = (*cell_io_ptrptr);

        while (curr_cell_io->next != NULL) {
            curr_cell_io = curr_cell_io->next;
        }
        curr_cell_io->next = new_cell_io;
    }
}

nodeptr create_node (char *name, int type, double nodeCap) {
    nodeptr new_node = calloc(1, sizeof(node));

    new_node->name = calloc(strlen(name) + 1, sizeof(char));
    strcpy(new_node->name, name);
    new_node->type = type;
    new_node->nodeCap = nodeCap;

    return new_node;
}

void add_ritem (ritemptr *ritem_list_ptr, rptr r) {
    ritemptr next = calloc(1, sizeof(ritem));
    next->r = r;

    // list has no items
    if (*ritem_list_ptr == NULL) {

        *ritem_list_ptr = next;

    } else {

    // list has some items, we need to find the end

        ritemptr i;

        for (i = *ritem_list_ptr; i->next != NULL; i = i->next);

        i->next = next;
    }

}
void process_subckt_inst(char **tokens, int num_toks, cell_io_ptr cell_io, struct hashlist **Nodehash, node_item_ptr **last_driver_ptr, int *numDrivers) {

    nodeptr curr_node = NULL;
    node_item_ptr next_src_item = NULL;

    int pin_type = 0;

    // find cell name in cell_io_list to know about pin order
    while(strcmp(cell_io->name, tokens[num_toks-1])) {
        cell_io = cell_io->next;

        if (cell_io == NULL) {
            fprintf(stderr, "Did not find stdcell %s in cell IO linked list.\n", tokens[num_toks-1]);
            exit(1);
        }
    }

    // Iterate over list of pins processing ones that were found in Liberty file
    // foreach pin
    //  -if not in hash, create and add to hash
    //  -if in hash, update connections, verify polarity / pin type correct, etc
    int i;
    // skip instance name (first token) and std cell name (last token)
    for (i = 1; i < num_toks-1; i++) {
        if (cell_io->pins[i-1] != NULL) {
            curr_node = HashLookup(tokens[i], Nodehash);

            if (cell_io->pins[i-1]->type == PIN_INPUT) {
                pin_type = SNK;
            } else if (cell_io->pins[i-1]->type == PIN_OUTPUT) {
                pin_type = SRC;
            } else {
                fprintf(stderr, "Pin type is not recognized\n");
            }

            if (curr_node == NULL) {
                // this is a new node we need to create and add to hash
                curr_node = create_node(tokens[i], pin_type, cell_io->pins[i-1]->cap);
                HashPtrInstall(curr_node->name, curr_node, Nodehash);
                printf("install new node\n");
                curr_node = NULL;
                curr_node = HashLookup(tokens[i], Nodehash);
            } else {
                if (    ((curr_node->type == SRC) && (pin_type == SNK))
                    ||  ((curr_node->type == SNK) && (pin_type == SRC))
                   ) {
                    fprintf(stderr, "Pin type for node %s changed polarity!\n", curr_node->name);
                }

                curr_node->type = pin_type;

                fprintf(stdout, "Node capacitance changed from %f to %f\n", curr_node->nodeCap, curr_node->nodeCap + cell_io->pins[i-1]->cap);
                curr_node->nodeCap += cell_io->pins[i-1]->cap;

            }

            // add node to list of drivers if the node is a SRC
            if (curr_node->type == SRC) {
                printf("found driver\n");
                next_src_item = calloc(1, sizeof(node_item));
                next_src_item->node = curr_node;
                **last_driver_ptr = next_src_item;
                *last_driver_ptr = &next_src_item->next;
                *numDrivers = *numDrivers + 1;
            }
        }
    }
}

double spiceValtoD(char *string) {
    char *endptr = string;

    double rtrnVal = 0;
    double suffix = 1;

    // find end of numbers
    int i = 0;

    while (string[i] != 0) {

        if (    string[i] == '.'
            ||  (string[i] >= 0x30 && string[i] <= 0x39)
           ) {
            i++;
        } else {
            i--;
            break;
        }
    }

    endptr += i * sizeof(char);

    rtrnVal = strtod(string, &endptr);

    if (endptr[0] == 'f') {
        suffix = 1E-15;
    } else if (endptr[0] == 'p') {
        suffix = 1E-12;
    } else if (endptr[0] == 'n') {
        suffix = 1E-9;
    } else if (endptr[0] == 'u') {
        suffix = 1E-6;
    } else if (endptr[0] == 'm') {
        suffix = 1E-3;
    } else if (endptr[0] == 'k') {
        suffix = 1E3;
    } else {
        suffix = 1;
    }

    rtrnVal *= suffix;

    return rtrnVal;
}

void process_r(char **tokens, int num_toks, struct hashlist **Nodehash, ritemptr *fullrlist) {
    // create ritem which captures the resistor and the connection between two nodes
    // for each node
    //      if node does not exist, create it
    //      add resistor to each node's list of resistors (connections to other nodes)
    //      add resistor to global list of resistors

    rptr    curr_r = NULL;
    nodeptr curr_node = NULL;


    curr_r = calloc(1, sizeof(r));
    curr_r->name = calloc(strlen(tokens[0]) + 1, sizeof(char));
    strcpy(curr_r->name, tokens[0]);
    curr_r->rval = spiceValtoD(tokens[num_toks-1]);

    int i;
    // skip instance name (first token) and resistance value (last token)
    for (i = 1; i < num_toks-1; i++) {
        curr_node = HashLookup(tokens[i], Nodehash);

        if (curr_node == NULL) {
            // this is a new node we need to create and add to hash
            curr_node = create_node(tokens[i], INT, 0);
            HashPtrInstall(curr_node->name, curr_node, Nodehash);
            printf("install new node\n");
            curr_node = NULL;
            curr_node = HashLookup(tokens[i], Nodehash);
        }

        if (i == 1) {
            curr_r->node1 = curr_node;
        } else {
            curr_r->node2 = curr_node;
        }
        add_ritem(&curr_node->rlist, curr_r);
    }

    add_ritem(fullrlist, curr_r);
}

void process_c(char **tokens, int num_toks, struct hashlist **Nodehash) {
    //
    // change capacitance units to farads
    //
    // for each node
    //      if node does not exist, create it
    //      add capacitance value to node capacitance total

    nodeptr curr_node = NULL;
    // keep all capacitance values in fF to match readliberty.c
    double cVal = spiceValtoD(tokens[num_toks-1]) * 1E15;

    int i;
    // skip instance name (first token) and capacitance value (last token)
    for (i = 1; i < num_toks-1; i++) {
        curr_node = HashLookup(tokens[i], Nodehash);

        if (curr_node == NULL) {
            // this is a new node we need to create and add to hash
            curr_node = create_node(tokens[i], INT, 0);
            HashPtrInstall(curr_node->name, curr_node, Nodehash);
            printf("install new node\n");
            curr_node = NULL;
            curr_node = HashLookup(tokens[i], Nodehash);
        }

        curr_node->nodeCap += cVal;
    }
}

// for multi-driver nets, must not recurse finding another driver
void sum_downstream_cap(nodeptr curr_node, nodeptr prev_node, short breadcrumbVal) {

    ritemptr curr_ritem = curr_node->rlist;

    while (curr_ritem != NULL) {
        // make sure to not backtrack to previous node
        // make sure to not recurse on the current node
        if (    (curr_ritem->r->node1 != prev_node)
            &&  (curr_ritem->r->node1 != curr_node)
           ) {

            sum_downstream_cap(curr_ritem->r->node1, curr_node, breadcrumbVal);
            curr_node->totCapDownstream += (curr_ritem->r->node1->totCapDownstream + curr_ritem->r->node1->nodeCap);

        } else if (     (curr_ritem->r->node2 != prev_node)
                    &&  (curr_ritem->r->node2 != curr_node)
           ) {

            sum_downstream_cap(curr_ritem->r->node2, curr_node, breadcrumbVal);
            curr_node->totCapDownstream += (curr_ritem->r->node2->totCapDownstream + curr_ritem->r->node2->nodeCap);

        }

        curr_ritem = curr_ritem->next;
    }
}

void add_snk (snkptr *snk_list_ptr, snkptr snk) {

    // list has no items
    if (*snk_list_ptr == NULL) {

        *snk_list_ptr = snk;

    } else {

    // list has some items, we need to find the end

        snkptr i;

        for (i = *snk_list_ptr; i->next != NULL; i = i->next);

        i->next = snk;
    }

}

void calculate_elmore_delay (
        nodeptr     curr_node,
        nodeptr     prev_node,
        rptr        prev_r, // the connection used to get curr_node
        elmdlyptr   curr_elmdly,
        /*snkptr      curr_snk,*/
        double      firstR,
        double      elmdly,
        short       breadcrumbVal
        ) {

    // -recursively walk each branch of nodes
    // -accumulate delay on each branch
    // -append to Elmore Delay list when sink node reached

    // accumulate delay
    // -first node uses a model resistor based on typical output drive strengths
    //  of stdcell librarie
    // -subsequent nodes us the resistor that was traveled to arrive at current
    //  node
    if (curr_node->type == SRC) {
        elmdly = firstR * (curr_node->nodeCap + curr_node->totCapDownstream);
    } else {
        elmdly = prev_r->rval * (curr_node->nodeCap + curr_node->totCapDownstream);
    }

    // -if current node is an input to another cell, this is an endpoint and the
    //  current delay value needs to be saved
    // -there still might be other connections though that need to be traversed
    //  to find other endpoints
    if (curr_node->type == SNK) {

        printf("Found SNK node %s with delay to it of %lf\n", curr_node->name, elmdly);
        snkptr curr_snk = calloc(1, sizeof(snk));

        curr_snk->snknode = curr_node;
        curr_snk->delay = elmdly;

        add_snk(&curr_elmdly->snklist, curr_snk);
    }

    ritemptr curr_ritem = curr_node->rlist;

    while (curr_ritem != NULL) {
        // make sure to not backtrack to previous node
        // make sure to not recurse on the current node
        if (    (curr_ritem->r->node1 != prev_node)
            &&  (curr_ritem->r->node1 != curr_node)
           ) {

            calculate_elmore_delay(curr_ritem->r->node1, curr_node, curr_ritem->r, curr_elmdly, firstR, elmdly, breadcrumbVal);

        } else if (     (curr_ritem->r->node2 != prev_node)
                    &&  (curr_ritem->r->node2 != curr_node)
           ) {

            calculate_elmore_delay(curr_ritem->r->node2, curr_node, curr_ritem->r, curr_elmdly, firstR, elmdly, breadcrumbVal);

        }

        curr_ritem = curr_ritem->next;
    }
}

int main (int argc, char* argv[]) {

    FILE* outfile = NULL;
    FILE* libfile = NULL;
    FILE* spcfile = NULL;

    int i, opt;
    int verbose = 0;

    Cell *cells, *newcell;
    Pin *newpin;
    char* libfilename;

    nodeptr currnode = NULL;
    // -Maintain a list of all nodes that are outputs / drivers.
    // -Iterate over the list to walk each interconnect to calculate
    //  Elmore Delay
    node_item_ptr drivers = NULL;
    node_item_ptr *last_driver = &drivers;
    int numDrivers = 0;

    // list of all Rs for debugging and to easily free them at end
    ritemptr allrs = NULL;

    struct hashlist *Nodehash[OBJHASHSIZE];

    /* See hash.c for these routines and variables */
    hashfunc = hash;
    matchfunc = match;

    /* Initialize net hash table */
    InitializeHashTable(Nodehash);

    // create first item in cell io list
    cell_io_ptr cell_io_list = NULL;

    while ((opt = getopt(argc, argv, "s:l:o:v:")) != -1) {
        switch (opt) {

        case 's':
            spcfile = fopen(optarg, "r");

            if (!spcfile) {
                fprintf(stderr, "Can't open outfile`%s': %s\n", optarg, strerror(errno));
            }
            break;

        case 'l':
            libfile = fopen(optarg, "r");
            libfilename = strdup(optarg);

            if (!libfile) {
                fprintf(stderr, "Can't open outfile`%s': %s\n", optarg, strerror(errno));
            }
            break;

        case 'o':
            if (!strcmp(optarg, "-")) {
                outfile = stdout;
            } else {
                outfile = fopen(optarg, "w");
            }
            if (!outfile) {
                fprintf(stderr, "Can't open outfile`%s': %s\n", optarg, strerror(errno));
            }
            break;

        case 'v':
            verbose = atoi(optarg);
            break;

        default:
            print_help();
            break;
        }
    }


    // Read in Liberty File
    printf("%s\n", libfilename);
    cells = read_liberty(libfilename, 0);

    if (verbose > 0) {
        for (newcell = cells; newcell; newcell = newcell->next) {
            fprintf(stdout, "Cell: %s\n", newcell->name);
            fprintf(stdout, "   Function: %s\n", newcell->function);
            for (newpin = newcell->pins; newpin; newpin = newpin->next) {
                fprintf(stdout, "   Pin: %s  cap=%g\n", newpin->name, newpin->cap);
            }
            fprintf(stdout, "\n");
        }
    }

    char *line;
    size_t nbytes = LIB_LINE_MAX;
    line = calloc(1, LIB_LINE_MAX);
    int bytesRead = 0;

    const char delims[3] = " \n";

    char **tokens;
    int num_toks = 0;

    bytesRead = getline(&line, &nbytes, spcfile);

    while (bytesRead > 0) {

        // skip blank lines
        if (bytesRead > 2) {
            tokens = tokenize_line(line, delims, &tokens, &num_toks);
            /*tokenize_line(line, delims, &tokens, &num_toks);*/

            if (!(strncmp(line, "R", 1))) {

                printf("located resistor line\n");
                process_r(tokens, num_toks, Nodehash, &allrs);

            } else if (!(strncmp(line, "C", 1))) {

                printf("located capacitor line\n");
                process_c(tokens, num_toks, Nodehash);

            } else if (!(strncmp(line, "X", 1))) {

                printf("located subckt instantiation line\n");
                printf("number of hash entries %d\n", RecurseHashTable(Nodehash, CountHashTableEntries));
                process_subckt_inst(tokens, num_toks, cell_io_list, Nodehash, &last_driver, &numDrivers);
                printf("number of hash entries %d\n", RecurseHashTable(Nodehash, CountHashTableEntries));

            } else if (!(strncmp(line, ".subckt", 7))) {
                printf("located subckt definition line\n");

                process_subckt_def(tokens, num_toks, cells, &cell_io_list);

                /*free(tokens);*/

                // read through the rest of the subckt definition
                while (strncmp(line, ".ends", 4)) { getline(&line, &nbytes, spcfile); }

                /*break;*/
            } else if (!(strncmp(line, "*", 1))) {
                printf("located comment line\n");
            }
        }

        /*free(tokens);*/
        bytesRead = getline(&line, &nbytes, spcfile);
    }

    // Walk each interconnect to calculate downstream capacitance at each node
    node_item_ptr curr_node_item = drivers;
    short breadcrumbVal = 1;

    elmdlyptr delays = calloc(numDrivers, sizeof(elmdly));

    int driverIndex = 0;

    printf("Number of drivers is %d\n", numDrivers);
    printf("Sum downstream capacitance for each node\n");
    printf("Calculate Elmore Delay for each driver\n");

    while (curr_node_item != NULL) {
        sum_downstream_cap(curr_node_item->node, NULL, breadcrumbVal);

        if (curr_node_item->node->type == SRC) {

            (&delays[driverIndex])->src = curr_node_item->node;

            calculate_elmore_delay(
                                    curr_node_item->node,
                                    NULL,
                                    NULL,
                                    &delays[driverIndex],
                                    /*NULL,*/
                                    1,
                                    0,
                                    breadcrumbVal
                                    );
            driverIndex++;
        }

        breadcrumbVal++;
        curr_node_item = curr_node_item->next;
    }

    currnode = HashFirst(Nodehash);

    while (currnode != NULL) {
        printf("%s\t\t%f\t%f\n", currnode->name, currnode->nodeCap, currnode->totCapDownstream);
        currnode = HashNext(Nodehash);
    }


    node_item_ptr curr_driver = drivers;

    while (curr_driver != NULL) {

        curr_driver = curr_driver->next;
    }

    elmdlyptr curr_elmdly = NULL;
    snkptr tmp_snk = NULL;
    snkptr curr_snk = NULL;

    for (driverIndex = 0; driverIndex < numDrivers; driverIndex++) {

        curr_elmdly = &delays[driverIndex];

        printf("%s\n%s %f\n", curr_elmdly->src->name, curr_elmdly->src->name, curr_elmdly->src->nodeCap + curr_elmdly->src->totCapDownstream);

        fprintf(outfile, "%s\n%s %f\n", curr_elmdly->src->name, curr_elmdly->src->name, curr_elmdly->src->nodeCap + curr_elmdly->src->totCapDownstream);

        curr_snk = curr_elmdly->snklist;

        while(curr_snk != NULL) {
            printf("%s %f\n", curr_snk->snknode->name, curr_snk->delay);
            fprintf(outfile, "%s %f\n", curr_snk->snknode->name, curr_snk->delay);
            curr_snk = curr_snk->next;
        }

        printf("\n");
        fprintf(outfile, "\n");

    }

    // Cleanup
    for (driverIndex = 0; driverIndex < numDrivers; driverIndex++) {

        curr_elmdly = &delays[driverIndex];

        curr_snk = curr_elmdly->snklist;

        while(curr_snk != NULL) {
            tmp_snk = curr_snk->next;
            free(curr_snk);
            curr_snk = tmp_snk;
        }
    }

    free(delays);

    ritemptr tmp_ritem = allrs;
    rptr tmp_r = NULL;
    int numRs = 0;

    while(allrs != NULL) {
        numRs++;
        tmp_ritem = allrs->next;
        free(allrs->r->name);
        free(allrs->r);
        free(allrs);
        allrs = tmp_ritem;
    }
    printf("Number of Rs: %d\n", numRs);

    currnode = HashFirst(Nodehash);
    int numNodes = 0;

    while (currnode != NULL) {
        numNodes++;
        numRs = 0;

        while(currnode->rlist != NULL) {
            numRs++;
            tmp_ritem = currnode->rlist->next;
            free(currnode->rlist);
            currnode->rlist = tmp_ritem;
        }

        printf("Node %s had %d Rs attached\n", currnode->name, numRs);
        free(currnode->name);
        free(currnode);
        currnode = HashNext(Nodehash);
    }
    printf("Number of nodes: %d\n", numNodes);

    free(line);

    while(drivers != NULL) {
        curr_driver = drivers->next;
        free(drivers);
        drivers = curr_driver;
    }

    cell_io_ptr tmp_cell_io;
    while(cell_io_list != NULL) {
        tmp_cell_io = cell_io_list->next;
        free(cell_io_list->name);
        free(cell_io_list->pins);
        free(cell_io_list);
        cell_io_list = tmp_cell_io;
    }
    delete_cell_list(cells);
    fclose(spcfile);
    fclose(libfile);
    fclose(outfile);

    return 0;
}
