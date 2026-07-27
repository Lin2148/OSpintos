#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static inline bool is_user_vaddr (const void *vaddr); 
void * pagedir_get_page (uint32_t *pd, const void *uaddr); 
static void check_valid_ptr (const void *vaddr);
static void check_valid_buffer (const void *buffer, unsigned size);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/**
 *  userprog傳參數 執行int $0x30觸發中斷請求,進入ker mode後 ,k執行此方法
 */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // 確認合法
  check_valid_ptr(f->esp);
  //讀取esp 指向stack top確認哪個中斷num
  int sys_num = *(int*) f->esp;  
  struct thread* t = thread_current();

  // 根據有的syscall寫對應操作
  switch (sys_num)
  {
  case SYS_HALT:
    shutdown_power_off();  //程式關閉了 沒後續
    break;

  // 關閉userprog  status參數存t屬性  沒回傳
  case SYS_EXIT:
    //status在stack往下4
    check_valid_ptr(f->esp+4);
    int status = *((int*) f->esp+1);  
    t->exit_code = status;
    thread_exit ();
    break;

  case SYS_EXEC:
    break;
  case SYS_WAIT:
    break;
  case SYS_CREATE:
    break;
  case SYS_REMOVE:
    break;
  case SYS_OPEN:
    break;
  case SYS_FILESIZE:
    break;
  case SYS_READ:
    break;
  case SYS_WRITE:
    //status在stack往下3參數
    //check_valid_ptr(f->esp+12);
    check_valid_ptr((int*)f->esp + 1);
    check_valid_ptr((int*)f->esp + 2);
    check_valid_ptr((int*)f->esp + 3);
    int fd = *((int*) f->esp+1);  
    void *buffer =*((void**)f->esp+2);
    unsigned size = *((unsigned*)f->esp + 3);
    
    // 先實作到console的部分
    check_valid_buffer(buffer, size);
    if (fd == 1){
      putbuf(buffer, size);
      f->eax = size;
    } 
    else{
      // ...
    }
    break;
  case SYS_SEEK:
    break;
  case SYS_TELL:
    break;
  case SYS_CLOSE:
    break;
  default:
    //exit
    printf ("system call!\n");
    thread_exit ();
    break;
  }

}

/**
 * 檢查userprog的ptr是否合法
 */
static void
check_valid_ptr (const void *vaddr)
{
  uint32_t *pd = thread_current ()->pagedir;
  if (!vaddr || !is_user_vaddr (vaddr) || pagedir_get_page (pd, vaddr) == NULL ){
    // syscall exit
    struct thread *t = thread_current();
    t->exit_code = -1; 
    thread_exit();
  }
}


/**
 * 檢查寫入的buffer沒有超過user prog
 */
static void
check_valid_buffer (const void *buffer, unsigned size)
{
  const char *ptr = (const char *) buffer;
  for (unsigned i = 0; i < size; i++){
    check_valid_ptr ((const void *) ptr);    
    ptr++; 
  }
}