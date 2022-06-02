#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
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

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
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
