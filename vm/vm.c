/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "kernel/hash.h" // project_3_수정 '<>'해도 된다
#include "userprog/exception.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
static struct frame_table ft; // project_3_수정
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED);
			   
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */

enum vm_type
page_get_type(struct page *page)
/*case에 뭐 추가해야하는교?*/
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:

		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux)
{
	ASSERT(VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */

		// 1. 페이지 생성
		struct page *uninit_page = (struct page *)malloc(sizeof(struct page));
		struct vm_initializer *initializer = NULL;

		// 2.  VM type에 따라 anon-initializer or filebacked-initializer,
		switch (type)
		{
		case VM_ANON:
			initializer = &anon_initializer;
			break;
		case VM_FILE:
			initializer = &file_backed_initializer;
			break;
		default:
			break;
		}

		// 2. uninit_initialize 설정
		uninit_new(uninit_page, upage, init, type, aux, initializer);

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, uninit_page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED) // project_3_수정
{
	struct page *page = NULL; // 현진의 생각 : NULL로 왜 초기화???
	struct hash_elem *he = hash_find(&spt->hst, &va);
	page = hash_entry(he, struct page, page_hash_elem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) // project_3_수정
{
	int succ = false;
	struct hash_elem *he = hash_find(&spt->hst, &page->page_hash_elem);
	if (he == NULL)
	{
		hash_insert(&spt->hst, he);
		succ = true;
	}
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void) // project_3_수정
{
	struct frame *frame = NULL;
	// 1. 물리 메모리에서 frame 할당
	frame = malloc(sizeof(struct frame));
	// 2. 디스크의 user pool에서 page 크기만큼 읽어오기
	void *k_page = palloc_get_page(PAL_USER);

	if (!k_page)
	{
		PANIC("todo");
	}
	// 3. 물리 메모리와 디스크에서 가져온 페이지 연결
	frame->kva = k_page; // kva(커널가상메모리)
	frame->page = NULL;

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	// 4. frame table로 frame 관리
	// 지금 frame 하나에 페이지가 할당되었으니.
	list_push_back(&ft.frame_list, frame);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// if (!check_address(addr) & user)
	// page =	spt_find_page(spt, addr);
	uint64_t pml4 = thread_current()->pml4;
	uint64_t *pte = pml4e_walk(pml4, (uint64_t)addr, 0);
	if ((*pte & PF_P) == not_present)
	{
		return vm_do_claim_page(page);
	}
	if ((*pte & PF_W) != write)
	{
		// vm_handle_wp
		exit(-1);
	}
	if ((*pte & PF_U) != user)
	{
		exit(-1);
	}
	return vm_do_claim_page(page); // 매핑과 페이지 초기화가 잘되었으면 true
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED) /*project_3_수정*/
{
	struct page *page = NULL;
	page = spt_find_page(&thread_current()->spt, va);
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
/*궁금1 : 페이지의 가상주소와 프레임의 가상주소를 페이지 테이블로 하여금 매핑을 하는데, */
/*frame -> page = page, page -> frame = frame 과 같이 직접적으로 연결해주는 이유가 뭘까요???*/
static bool vm_do_claim_page(struct page *page) // project_3_수정
{
	if (page == NULL)
		return false;

	struct frame *frame = vm_get_frame();

	// /* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	uint64_t *pml4 = thread_current()->pml4;
	if (pml4_set_page(pml4, page->va, frame->kva, 1))
	{
		return swap_in(page, frame->kva);
	}
	else
	{
		return false;
	}
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->hst, page_hash, page_less, NULL); // project_3_수정
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	struct hash_iterator *i = malloc(sizeof(struct hash_iterator));
	hash_first(i->elem, &src->hst);
	// 1. src의 각 페이지를 돌며
	while (hash_next(&i))
	{
		// 2. dst에 정확하게 복사
		struct page *hte = hash_entry(hash_cur(&i), struct page, page_hash_elem);
		spt_insert_page(&dst->hst, hte);
		// 3. uninit page 할당하고 바로 claim
		vm_claim_page(hte->va);
	}
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. dirty bit 나올때 다시 볼것 */ 
	// hash_apply(&spt->hst, &spt_remove_page);
	hash_destroy(&spt->hst, &spt_remove_page);
}
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED)
{ // project_3_수정
	const struct page *p = hash_entry(p_, struct page, page_hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED)
{ // project_3_수정
	const struct page *a = hash_entry(a_, struct page, page_hash_elem);
	const struct page *b = hash_entry(b_, struct page, page_hash_elem);
	return a->va < b->va;
}