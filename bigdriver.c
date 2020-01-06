//Andrew Imredy
//api5
#include <stdio.h>
#include <unistd.h>

#include "mymalloc.h"

// Ooooh, pretty colors...
#define CRESET   "\e[0m"
#define CRED     "\e[31m"
#define CGREEN   "\e[32m"
#define CYELLOW  "\e[33m"
#define CBLUE    "\e[34m"
#define CMAGENTA "\e[35m"
#define CCYAN    "\e[36m"

// You can use these in printf format strings like:
//        printf("This is " RED("red") " text.\n");
// NO COMMAS!!

#define RED(X)     CRED X CRESET
#define GREEN(X)   CGREEN X CRESET
#define YELLOW(X)  CYELLOW X CRESET
#define BLUE(X)    CBLUE X CRESET
#define MAGENTA(X) CMAGENTA X CRESET
#define CYAN(X)    CCYAN X CRESET

#define PTR_ADD_BYTES(ptr, byte_offs) ((void*)(((char*)(ptr)) + (byte_offs)))

void* start_test(const char* where) {
	printf(CYAN("---------------------------------------------------------------------------\n"));
	printf(CYAN("Running %s...\n"), where);
	return sbrk(0);
}

void check_heap_size(const char* where, void* heap_at_start) {
	void* heap_at_end = sbrk(0);
	unsigned int heap_size_diff = (unsigned int)(heap_at_end - heap_at_start);

	if(heap_size_diff)
		printf(RED("After %s the heap got bigger by %u (0x%X) bytes...\n"),
			where, heap_size_diff, heap_size_diff);
	else
		printf(GREEN("Yay, after %s, everything was freed!\n"), where);
}

void fill_array(int* arr, int length) {
	int i;

	for(i = 0; i < length; i++)
		arr[i] = i + 1;
}

int* make_array(int length) {
	int* ret = my_malloc(sizeof(int) * length);
	fill_array(ret, length);
	return ret;
}

// A very simple test that allocates two blocks, fills them with data,
// then frees them in reverse order. Even the simplest allocator should
// work for this, and the heap should be back where it started afterwards.
void test_writing() {
	void* heap_at_start = start_test("test_writing");
	printf(YELLOW(
		"If this crashes, make sure my_malloc returns a pointer to\n"
		"the data part of the block, NOT the header. my_free also\n"
		"has to handle that by moving the pointer backwards.\n"));
	printf("!!! " RED("RUN OTHER TESTS TOO. THIS IS NOT THE ONLY TEST.") " !!!\n");

	int* a = make_array(10);
	int* b = make_array(10);

	// If the contents of one of these arrays is corrupted, chances are your second
	// malloc screwed up the memory that was returned by the first.
	int i;
	for(i = 0; i < 10; i++)
		printf("a[%d] = %d, b[%d] = %d\n", i, a[i], i, b[i]);

	// Freeing in reverse order.
	my_free(b);
	my_free(a);

	check_heap_size("test_writing", heap_at_start);
}

// A slightly more complex test that makes sure you can deallocate in either order and
// that those deallocated blocks can be reused.
void test_reuse() {
	void* heap_at_start = start_test("test_reuse");
	int* a = make_array(20);
	int* b = make_array(20);

	// Now our heap has two blocks: [U 80][U 80]

	my_free(a);

	// After that free, your heap should have two blocks: [F 80][U 80]

	// So when we allocate another block of the same size,
	// it should reuse the first one:

	int* c = make_array(20);

	if(a != c)
		printf(RED("You didn't reuse the free block!\n"));

	// Again, we have two blocks: [U 80][U 80]

	my_free(c);
	print_heap();

	// And now we have: [F 80][U 80]

	my_free(b);

	// Finally, if you DIDN'T implement coalescing, you
	// will still have one free 80-byte block on the heap,
	// and you'll get a message saying the heap grew
	// by like, 96 bytes.

	// But if you DID implement coalescing, you should have
	// nothing left on the heap here!

	check_heap_size("test_reuse", heap_at_start);
}

// Makes sure that your coalescing works.
void test_coalescing() {
	void* heap_at_start = start_test("test_coalescing");
	int* a = make_array(10);
	int* b = make_array(10);
	int* c = make_array(10);
	int* d = make_array(10);
	int* e = make_array(10);

	// Heap should be:
	// [U 40][U 40][U 40][U 40][U 40]

	// Now let's test freeing. The first free will test
	// freeing at the beginning of the heap, and the next
	// ones will test with a single previous free neighbor.

	// After each of these frees, you should have ONE
	// free block at the beginning (sizes below assume your headers are 16 bytes):
	my_free(a); // [F  40][U 40][U 40][U 40][U 40]
	my_free(b); // [F  96      ][U 40][U 40][U 40]
	my_free(c); // [F 152            ][U 40][U 40]
	my_free(d); // [F 208                  ][U 40]

	// This should reuse a's block, since it's 208 bytes.
	int* f = make_array(52);

	if(a != f)
		printf(RED("You didn't reuse the coalesced block!\n"));

	// Now, when we free these, they should coalesce into
	// a single big block, and then be sbrk'ed away!
	my_free(f);
	my_free(e);

	// ...and the break should be back to where it started.
	check_heap_size("part 1 of test_coalescing", heap_at_start);

	// Let's re-allocate...
	a = make_array(10);
	b = make_array(10);
	c = make_array(10);
	d = make_array(10);
	e = make_array(10);

	// Now let's test freeing random blocks. my_free(a)
	// will test freeing with a single next free neighbor, and
	// my_free(c) will test with two free neighbors.

	// After each you should have:
	my_free(b); // [U  40][F 40][U 40][U 40][U 40]
	my_free(d); // [U  40][F 40][U 40][F 40][U 40]
	my_free(a); // [F  96      ][U 40][F 40][U 40]
	my_free(c); // [F 208                  ][U 40]
	my_free(e); // nothing left!

	check_heap_size("part 2 of test_coalescing", heap_at_start);

	// Finally, let's make sure coalescing at the beginning
	// and end of the heap work properly.

	a = make_array(10);
	b = make_array(10);
	c = make_array(10);
	d = make_array(10);
	e = make_array(10);

	// After each you should have:
	my_free(b); // [U 40][F 40][U 40][U 40][U 40]
	my_free(a); // [F 96      ][U 40][U 40][U 40]
	my_free(d); // [F 96      ][U 40][F 40][U 40]
	my_free(e); // [F 96      ][U 40]
	my_free(c); // nothing left!

	check_heap_size("part 3 of test_coalescing", heap_at_start);
}

// Makes sure that your block splitting works.
void test_splitting() {
	void* heap_at_start = start_test("test_splitting");

	int* medium = make_array(64); // make a 256-byte block.
	int* holder = make_array(4);  // holds the break back.
	my_free(medium);

	// [F 256][U 16]

	// THIS allocation SHOULD NOT split the block, since it's
	// too big (would leave a too-small block).
	// It would want a 228 byte data portion, + 16 bytes for the header, would leave
	// only 8 bytes for the free split block's data.

	int* too_big = make_array(57);
	// [U 256][U 16]

	// Now you should have two blocks on the heap, but the first used one should STILL
	// be 256 bytes, even though the user only asked for 228!

	my_free(too_big);

	// [F 256][U 16]

	// Let's see if your algorithm can split blocks.

	// After each, you should be left with a free block of the given
	// size:
	int* tiny1 = make_array(4); // [U 16][F 224][U 16]
	int* tiny2 = make_array(4); // [U 16][U 16][F 192][U 16]
	int* tiny3 = make_array(4); // [U 16][U 16][U 16][F 160][U 16]
	int* tiny4 = make_array(4); // [U 16][U 16][U 16][U 16][F 128][U 16]

	if(tiny1 != medium)
		printf(RED("You didn't split the 256B block!\n"));
	else if(tiny2 != PTR_ADD_BYTES(tiny1, 32))
		printf(RED("You didn't split the 224B block!\n"));
	else if(tiny3 != PTR_ADD_BYTES(tiny2, 32))
		printf(RED("You didn't split the 192B block!\n"));
	else if(tiny4 != PTR_ADD_BYTES(tiny3, 32))
		printf(RED("You didn't split the 160B block!\n"));

	my_free(tiny1);
	my_free(tiny2);
	my_free(tiny3);
	my_free(tiny4);
	my_free(holder);
	check_heap_size("test_splitting", heap_at_start);
}

// A test which ensures that worst-fit works how it should.
void test_worst_fit() {
	void* heap_at_start = start_test("test_worst_fit");
	printf(YELLOW("This test assumes you have coalescing and splitting working!\n"));
	int* a    = make_array(10);
	int* div1 = make_array(1);
	int* b    = make_array(20);
	int* div2 = make_array(1);
	int* c    = make_array(30);
	int* div3 = make_array(1);
	int* d    = make_array(40);
	int* div4 = make_array(1);
	int* e    = make_array(50);
	int* div5 = make_array(1);
	my_free(a);
	my_free(b);
	my_free(c);
	my_free(d);
	my_free(e);

	// Should have 5 free blocks, separated by tiny (16B) used blocks, like so:
	// [F 40][U 16][F 80][U 16][F 120][U 16][F 160][U 16][F 200][U 16]

	// The biggest free block is 200 bytes, at 'e'. So the next allocation should
	// go there, regardless of size.

	int* should_be_e = make_array(30);

	if(should_be_e != e) {
		printf(RED("should have reused the biggest 200-byte block.\n"));
	} else {
		// You correctly reused the block at 'e'. The heap should be like:
		// [F 40][U 16][F 80][U 16][F 120][U 16][F 160][U 16][U 120][F 64][U 16]

		// If we malloc 160 bytes, worst-fit should find that 160-byte free block at d

		int* should_be_d = make_array(40);

		if(should_be_d != d) {
			printf(RED("the 40-byte block was not reused.\n"));
			my_free(should_be_d);
		} else {
			// You correctly reused the block at 'd'. The heap should be like:
			// [F 40][U 16][F 80][U 16][F 120][U 16][U 160][U 16][U 120][F 64][U 16]
			// and if we allocate a 10-int array... it should go to c.

			int* should_be_c = make_array(10);

			if(should_be_c != c) {
				printf(RED("the 80-byte block was not reused.\n"));

				if(should_be_c > div5) {
					printf(RED("looks like you expanded the heap instead...\n"));
				}
			}

			my_free(should_be_d);
			my_free(should_be_c);
		}
	}

	my_free(should_be_e);
	my_free(div1);
	my_free(div2);
	my_free(div3);
	my_free(div4);
	my_free(div5);
	check_heap_size("test_worst_fit", heap_at_start);
}


int main() {
	void* heap_at_start = sbrk(0);

	// Uncomment a test and recompile before running it.
	// When complete, you should be able to uncomment all the tests
	// and they should run flawlessly.

	test_writing();
	test_reuse();
	test_coalescing();
	test_splitting();
	test_worst_fit();

	// Just to make sure!
	check_heap_size("main", heap_at_start);
	return 0;
}