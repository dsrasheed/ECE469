// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pde_t pde;
	pte_t pte;

	if (err == FEC_WR)
		panic("pgfault: pgfault did not occur on a write");
	
	pde = uvpd[PDX(addr)];
	if ((pde & (PTE_P|PTE_U)) != (PTE_P|PTE_U))
		panic("pgfault: address has no page directory entry.\n");

	pte = uvpt[PGNUM(addr)];
	if ((pte & (PTE_P|PTE_U|PTE_COW)) != (PTE_P|PTE_U|PTE_COW))
		panic("pgfault: page table entry is not copy on write.\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	if ((r = sys_page_alloc(0, (void*) PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault: sys_page_alloc: %e", r);

	memmove((void*) PFTEMP, (void*) PTE_ADDR(addr), PGSIZE);

	if ((r = sys_page_map(0, (void*) PFTEMP, 0, (void*) PTE_ADDR(addr), PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	
	if ((r = sys_page_unmap(0, (void*) PFTEMP)) < 0)
		panic("pgfault: sys_page_unmap: %e", r);

	return;
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	uintptr_t addr;
	pte_t pte;

	pte = uvpt[pn];
	addr = pn * PGSIZE;

	if ((pte & (PTE_W | PTE_COW)) != 0) {
		if (sys_page_map(0, (void*) addr, envid, (void*) addr, PTE_P | PTE_U | PTE_COW) < 0)
			panic("duppage: Failed to map parent page into child process");
		if (sys_page_map(0, (void*) addr, 0, (void*) addr, PTE_P | PTE_U | PTE_COW) < 0)
			panic("duppage: Failed to change parent page to copy-on-write");
	}
	else
		if (sys_page_map(0, (void*) addr, envid, (void*) addr, PTE_P | PTE_U) < 0)
			panic("duppage: Failed to map parent page into child process");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
	pte_t pte;
	pde_t pde;

	set_pgfault_handler(pgfault);

	if ((envid = sys_exofork()) < 0)
		panic("fork: sys_exofork failed");

	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	extern void _pgfault_upcall(void);
	if (sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0)
		panic("fork: failed to set pgfault upcall");

	if (sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W) < 0)
		panic("fork: failed to alloc new user exception stack");
	
	for (unsigned i = UTEXT; i < UTOP; i += PGSIZE) {
		if (i == (UXSTACKTOP - PGSIZE))
			continue;

		pde = uvpd[PDX(i)];
		if ((pde & PTE_P) == 0) continue;

		pte = uvpt[PGNUM(i)];
		if ((pte & (PTE_P | PTE_U)) == (PTE_P | PTE_U)) {
			duppage(envid, PGNUM(i));
		}
	}

	if (sys_env_set_status(envid, ENV_RUNNABLE) < 0)
		panic("fork: failed to set env status");

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
