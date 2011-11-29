#include <stdlib.h>

#if !defined(__GNUC__)

static inline void malloc_verify_start(void)
{
}

static inline void malloc_verify_end(void)
{
}

#else

#include <malloc.h>
#include <stdio.h>

typedef struct malloc_verify_state_s {
	struct {
		void *(*saved_malloc)(size_t size, const void * caller);
		void *(*saved_realloc)(void * ptr, size_t size, const void * caller);
		void (*saved_free)(void * ptr, const void * caller);
		void *(*saved_memalign)(size_t alignment, size_t size, const void * caller);
	} hooks;
	size_t nallocs, nfrees;
} malloc_verify_state_s;

static malloc_verify_state_s malloc_verify_state = {
	.hooks = {NULL, NULL, NULL, NULL},
	.nallocs = 0,
	.nfrees = 0
};

static void *
malloc_verify_malloc(size_t size, const void * caller);

static void *
malloc_verify_realloc(void * ptr, size_t size, const void * caller);

static void
malloc_verify_free(void * ptr, const void * caller);

static void *
malloc_verify_memalign(size_t alignment, size_t size, const void * caller);

static void
malloc_verify_install_hooks(void)
{
	__malloc_hook = malloc_verify_malloc;
	__realloc_hook = malloc_verify_realloc;
	__free_hook = malloc_verify_free;
	__memalign_hook = malloc_verify_memalign;
}

static void
malloc_verify_restore_hooks(void)
{
	__malloc_hook = malloc_verify_state.hooks.saved_malloc;
	__realloc_hook = malloc_verify_state.hooks.saved_realloc;
	__free_hook = malloc_verify_state.hooks.saved_free;
	__memalign_hook = malloc_verify_state.hooks.saved_memalign;
}

static void *
malloc_verify_malloc(size_t size, const void * caller)
{
	malloc_verify_restore_hooks();
	void * ptr = malloc(size);
	if (ptr) malloc_verify_state.nallocs ++;
	malloc_verify_install_hooks();
	
	return ptr;
}

static void *
malloc_verify_realloc(void * ptr, size_t size, const void * caller)
{
	malloc_verify_restore_hooks();
	void * new_ptr = realloc(ptr, size);
	if (new_ptr && !ptr) malloc_verify_state.nallocs ++;
	malloc_verify_install_hooks();
	
	return new_ptr;
}

static void
malloc_verify_free(void * ptr, const void * caller)
{
	malloc_verify_restore_hooks();
	if (ptr) malloc_verify_state.nfrees ++;
	free(ptr);
	malloc_verify_install_hooks();
}

static void *
malloc_verify_memalign(size_t alignment, size_t size, const void * caller)
{
	malloc_verify_restore_hooks();
	malloc_verify_state.nallocs ++;
	void * ptr = memalign(alignment, size);
	malloc_verify_install_hooks();
	
	return ptr;
}

static void
malloc_verify_start(void)
{
	malloc_verify_state.hooks.saved_malloc = __malloc_hook;
	malloc_verify_state.hooks.saved_realloc = __realloc_hook;
	malloc_verify_state.hooks.saved_free = __free_hook;
	malloc_verify_state.hooks.saved_memalign = __memalign_hook;
	
	malloc_verify_install_hooks();
}

static void
malloc_verify_end(void)
{
	malloc_verify_restore_hooks();
	if (malloc_verify_state.nallocs != malloc_verify_state.nfrees) {
		fprintf(stderr, "memory leak detected\n");
		abort();
	}
}

#endif
