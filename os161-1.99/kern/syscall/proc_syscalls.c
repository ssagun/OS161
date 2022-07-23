#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <clock.h>
#include <array.h>
#include <limits.h>
#include <vfs.h>
#include <test.h>
#include "opt-A2.h"

/* this implementation of sys__exit does not do anything with the exit code */
/* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

    struct addrspace *as;

    struct proc *p = curproc;
    /* for now, just include this to keep the compiler from complaining about
       an unused variable */
    (void)exitcode;

    DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

    KASSERT(curproc->p_addrspace != NULL);
    as_deactivate();
    /*
     * clear p_addrspace before calling as_destroy. Otherwise if
     * as_destroy sleeps (which is quite possible) when we
     * come back we'll be calling as_activate on a
     * half-destroyed address space. This tends to be
     * messily fatal.
     */
    as = curproc_setas(NULL);
    as_destroy(as);

#if OPT_A2
    // for(unsigned i = 0; i < array_num(&p->p_children); i++) {
  unsigned i = 0;
  while(array_num(p->p_children)) {
      struct proc *temp_child = array_get(p->p_children, i);
      array_remove(p->p_children, i);
      spinlock_acquire(&temp_child->p_lock);
      if(temp_child->p_exitstatus == 1) {
          spinlock_release(&temp_child->p_lock);
          proc_destroy(temp_child);
      }
      else {
          temp_child->p_parent = NULL;
          spinlock_release(&temp_child->p_lock);
      }
  }
#endif

    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_remthread(curthread);

#if OPT_A2
    spinlock_acquire(&p->p_lock);
  if(p->p_parent != NULL && p->p_parent->p_exitstatus == 0) {
      p->p_exitstatus = 1;
      p->p_exitcode = exitcode;
      spinlock_release(&p->p_lock);
  }
  else{
      spinlock_release(&p->p_lock);
      proc_destroy(p);
  }
#else

    /* if this is the last user process in the system, proc_destroy()
       will wake up the kernel menu thread */
    proc_destroy(p);
#endif

    thread_exit();
    /* thread_exit() does not return, so we should never get here */
    panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
    /* for now, this is just a stub that always returns a PID of 1 */
    /* you need to fix this to make it work properly */
#if OPT_A2
    *retval = curproc->p_pid;
#else
    *retval = 1;
#endif
    return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
            userptr_t status,
            int options,
            pid_t *retval)
{
    int exitstatus;
    int result;

    /* this is just a stub implementation that always reports an
       exit status of 0, regardless of the actual exit status of
       the specified process.
       In fact, this will return 0 even if the specified process
       is still running, and even if it never existed in the first place.

       Fix this!
    */

#if OPT_A2
    struct proc *p = curproc;
  struct proc *temp_child;
  struct proc *iterator;
  unsigned i;
  int flag = 0;
  for(i = 0; i < array_num(p->p_children); i++) {
      iterator = array_get(p->p_children, i);
      if(iterator->p_pid == pid) {
          flag = 1;
          temp_child = array_get(p->p_children, i);
          array_remove(p->p_children, i);
          break;
      }
  }
  if(flag == 0) {
      panic("Bruh give valid PID");
  }

  spinlock_acquire (& temp_child ->p_lock );
  while (!temp_child->p_exitstatus) {
      spinlock_release (& temp_child ->p_lock );
      clocksleep (1);
      spinlock_acquire (& temp_child ->p_lock );
      }
  spinlock_release (& temp_child ->p_lock );

  exitstatus = temp_child->p_exitcode;
  proc_destroy(temp_child);

  exitstatus = _MKWAIT_EXIT(exitstatus);
#endif

    if (options != 0) {
        return(EINVAL);
    }
    /* for now, just pretend the exitstatus is 0 */
    // exitstatus = 0;
    result = copyout((void *)&exitstatus,status,sizeof(int));
    if (result) {
        return(result);
    }
    *retval = pid;
    return(0);
}

#if OPT_A2
int sys_fork(pid_t *retval, struct trapframe *tf) {
    struct proc *nproc;
    nproc = proc_create_runprogram("child");
    nproc->p_parent = curproc;
    // if(proc == NULL) {}
    struct addrspace *child_as;
    as_copy(curproc_getas(), &child_as);
    nproc->p_addrspace = child_as;

    struct trapframe *trapframe_for_child = kmalloc(sizeof(struct trapframe));
    memcpy(trapframe_for_child, tf, sizeof(struct trapframe));

    thread_fork("child_thread", nproc, (void *)&enter_forked_process,
                          (struct trapframe *)trapframe_for_child, 0);
    array_add(curproc->p_children, nproc, NULL);

    *retval = nproc->p_pid;

    clocksleep (1);
    return 0;
}
#endif

char **args_alloc(char **argv) {
    int n = 0;
    while(argv[n] != NULL) {
        n++;
    }
    char **arg = kmalloc((n + 1) * sizeof(char *));
    for(int i = 0; i < n; i++) {
        size_t l = strlen(argv[i]) + 1;
        arg[i] = kmalloc(l * sizeof(char));
    }

    return arg;
}

void args_free(char **arg) {
    int n = 0;
    while(arg[n] != NULL) {
        n++;
    }
    for(int i = 0; i < n; i++) { // fix this
        kfree(arg[i]);
    }
    kfree(arg);
}

int argcopy_in(char ** args, char **argv) {
    int s = 0;
    while(argv[s] != NULL) {
        s++;
    }
    for(int i = 0; i <= s; i++) { // <=?
        size_t l = strlen(argv[i]) + 1;
        copyinstr((const_userptr_t) argv[i], args[i], l * sizeof(char), &l);
    }
    args[s+1] = NULL;
    return s;
}


int sys_execv(char *progname, char **argv) {
    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    int nargs = 0;
    while(argv[nargs] != NULL) {
        nargs++;
    }

    char **args = args_alloc(argv);
    argcopy_in(args, argv);

    /* Open the file. */
    result = vfs_open(progname, O_RDONLY, 0, &v);
    if (result) {
        return result;
    }

    /* We should be a new process. */
    KASSERT(curproc_getas() == NULL);
    /* Create a new address space. */

    struct addrspace *oas = curproc_getas();

    as = as_create();
    if (as ==NULL) {
        vfs_close(v);
        return ENOMEM;
    }

    /* Switch to it and activate it. */
    curproc_setas(as);
    as_activate();

    // copy addrspace here

    /* Load the executable. */
    result = load_elf(v, &entrypoint);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        vfs_close(v);
        return result;
    }

    /* Done with the file now. */
    vfs_close(v);

    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        return result;
    }

    //enter stuff here
    vaddr_t skptrc = stackptr;
    vaddr_t *argv_user = kmalloc((nargs + 1) * sizeof(vaddr_t));

    for(int i = nargs-1; i >= 0; i--) {
        skptrc =  argcopy_out(skptrc, args[i]);
        argv_user[i] = skptrc;
    }
    argv_user[nargs] = (vaddr_t) NULL;

    for( int i = nargs; i >= 0; i--) {
        size_t vaddrs = sizeof(vaddr_t);
        skptrc -= vaddrs;
        copyout((void *) &argv_user[i], (userptr_t) skptrc, vaddrs);
    }

    vaddr_t ba = USERSTACK;

    vaddr_t ap = skptrc;

    vaddr_t offset = ROUNDUP(USERSTACK - skptrc,8);

    skptrc = ba - offset;

    args_free(args);

    as_deactivate();
    as_destroy(oas);
    as_activate();

    /* Warp to user mode. */
    enter_new_process(nargs /*argc*/, (userptr_t) ap /*userspace addr of argv*/,
                      skptrc, entrypoint);


    /* enter_new_process does not return. */
    panic("enter_new_process returned\n");
    return EINVAL;
}
