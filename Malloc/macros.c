
    /* ================================================= Macros Provided in the assignment tips ==============================================================*/

// This assumes you have a struct or typedef called "block_header" and "block_footer"
#define OVERHEAD (sizeof(block_header)+sizeof(block_footer))
// Given a payload pointer, get the header or footer pointer
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-OVERHEAD)
// Given a payload pointer, get the next or previous payload pointer
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-OVERHEAD))
// ******These macros assume you are using a size_t for headers and footers ******
// Given a pointer to a header, get or set its value
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))
// Combine a size and alloc bit
#define PACK(size, alloc) ((size) | (alloc))
// Given a header pointer, get the alloc or size
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_SIZE(p) (GET(p) & ~0xF)

// ******These macros assume you are using a struct for headers and footers*******
// Given a header pointer, get the alloc or size
#define GET_ALLOC(p) ((block_header *)(p))->allocated
#define GET_SIZE(p) ((block_header *)(p))->size

// ******Recommended helper functions ******

/* These functions will provide a high-level recommended structure to your program.
* Fill them in as needed, and create additional helper functions depending on your design.
*/