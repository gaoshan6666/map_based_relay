
#include <linux/type.h>
#include <linux/list.h>

/**
 * Use shared memory to store the neighbor list. Because the memory addr should be continuous,
 * the accessing of the table could use index form.
 *
 * From start of shared mem, 1byte to determine if it is updating,
 * 4byte (sizeof(int)) to store the number of the neighbors.
 *
 */
#define NEIGH_STATUS_OFFSET 0
#define NEIGH_COUNT_OFFSET	1
#define NEIGH_DATA_OFFSET	5

struct neighbor_table_ {
	u_int32_t ip;
	double longitude;
	double latitude;
	uint8_t geoHash[3];
	unsigned char idStr_section[25];//this node's closest road section
	int		isvalid;
};

typedef struct neighbor_table_ neighbor_table;


