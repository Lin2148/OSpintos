#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/malloc.h"


static void syscall_handler (struct intr_frame *);
static inline bool is_user_vaddr (const void *); 
void * pagedir_get_page (uint32_t *, const void *); 
static void check_valid_ptr (const void *);
static void check_valid_buffer (const void *, unsigned );
static struct file* get_file_by_fd(int fd);
void sys_exit(int);

struct lock syscall_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  // lock init
  lock_init(&syscall_lock);

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
    {
      shutdown_power_off();  //程式關閉了 沒後續
      break;

    }
    // 關閉userprog  status參數存t屬性  沒回傳
    case SYS_EXIT:
    {
      //status在stack往下4
      check_valid_ptr(f->esp+4);
      int status = *((int*) f->esp+1);  
      sys_exit(status);
      break;
    }
    case SYS_EXEC:
    {
      break;
    }
    case SYS_WAIT:
    {
      break;
    }
      case SYS_CREATE:
    {
      //檢查2參數位置
      check_valid_ptr((const char*)f->esp + 1);
      check_valid_ptr((int*)f->esp + 2);
      const char *file = *((char**)f->esp+1);
      unsigned initial_size = *((unsigned*)f->esp + 2);

      //檢查file ptr位置 非null且在usrprog內
      if (file == NULL){
        sys_exit(-1);
      }
      check_valid_ptr((void*)file);

      lock_acquire(&syscall_lock);
      f->eax = filesys_create (file, initial_size);
      lock_release(&syscall_lock);

      break;
    }
    case SYS_REMOVE:
    {
      check_valid_ptr((const char*)f->esp + 1);
      const char *file = *((char **)f->esp+1);
      check_valid_ptr((const void*)file);

      lock_acquire(&syscall_lock);
      f->eax = filesys_remove(file);
      lock_release(&syscall_lock);
      break;
    }
    case SYS_OPEN:
    {
      check_valid_ptr((const char*)f->esp + 1);
      const char *file_name = *((char**)f->esp + 1);
      check_valid_ptr((const void*)file_name);
      
      lock_acquire(&syscall_lock);
      struct file *file_obj = filesys_open(file_name);
      lock_release(&syscall_lock);
      
      // 開啟失敗
      if (file_obj == NULL){
        f->eax = -1;
      }  else{
        // file方法開啟成功 建立file_elem 結構 存fd進去
        struct file_elem *new_file = malloc(sizeof(struct file_elem));
        if (new_file == NULL){
          // malloc失敗   把file close
          lock_acquire(&syscall_lock);
          filesys_remove(file_name);
          lock_release(&syscall_lock);

          f->eax = -1;
        } else{
          // file_elem裡面屬性填入，thread fd加一
          new_file->fd = t->next_fd;
          new_file->file_ptr = file_obj;
          t->next_fd++;

          //  存入t的table
          list_push_back(&t->file_descriptor, &new_file->elem);

          // 回傳值 = fd
          f->eax = new_file->fd;
        }
      } 
      break;
    }
    case SYS_FILESIZE:
    {
      check_valid_ptr((int*)f->esp + 1);
      int fd = *((int*)f->esp + 1);

      // 找不到
      struct file* file = get_file_by_fd(fd);
      if ( file == NULL){
        sys_exit(-1);
      }
      // 找到
      lock_acquire(&syscall_lock);
      f->eax = file_length(file);
      lock_release(&syscall_lock);
      break;
    }
    case SYS_READ:
    {
      break;
    }
    case SYS_WRITE:
    {
      //status在stack往下3參數
      check_valid_ptr((int*)f->esp + 1);
      check_valid_ptr((int*)f->esp + 2);
      check_valid_ptr((int*)f->esp + 3);
      int fd = *((int*) f->esp+1);  
      void *buffer =*((void**)f->esp+2);
      unsigned size = *((unsigned*)f->esp + 3);
      
      // 先實作寫到console的部分
      check_valid_buffer(buffer, size);
      //Fd 1 writes to the console.
      if (fd == 1){
        putbuf(buffer, size);
        f->eax = size;
      } 
      else{
        struct file *file = get_file_by_fd(fd);
        // fd=0代表寫入sysin 錯誤狀況
        if (file == NULL) {
          f->eax = 0;
          break;
        }
        // 正常寫入
        lock_acquire(&syscall_lock);
        f->eax = file_write(file, buffer, size);
        lock_release(&syscall_lock);
      }
      break;
    }
    case SYS_SEEK:
    {
      break;
    }
    case SYS_TELL:
    {
      check_valid_ptr((int*)f->esp + 1);
      int fd = *((int*)f->esp + 1);

      //  fd反找file
      struct file* cur_file = get_file_by_fd(fd);
      if ( cur_file == NULL){
        sys_exit(-1);
      }

      lock_acquire(&syscall_lock);
      f->eax = file_tell(cur_file);
      lock_release(&syscall_lock);
      break;
    }
    case SYS_CLOSE:
    {
      check_valid_ptr((int*)f->esp + 1);
      int fd = *((int*)f->esp + 1);

      if (fd ==1 || fd==0){
        break;
      }
  
      struct thread *cur = thread_current();
      struct list_elem *e;

      if (fd ==1 || fd==0){
        return;
      }
      // 找list 看有無開啟的file 有回傳
      for (e = list_begin(&cur->file_descriptor); e != list_end(&cur->file_descriptor); e = list_next(e)){
        struct file_elem *fe = list_entry (e, struct file_elem, elem);
        if(fe->fd == fd){
          lock_acquire(&syscall_lock);
          file_close(fe->file_ptr);
          lock_release(&syscall_lock);

          list_remove(e);
          free(fe);
          break;
        }
      }
      break;
    }
    default:
    {
      //exit
      printf ("system call!\n");
      thread_exit ();
      break;
    }
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
    sys_exit(-1);
  }
}


/**
 * 檢查寫入的buffer沒有超過user prog
 */
static void
check_valid_buffer (const void *buffer, unsigned size)
{
  //char flow add 1 byte then check valid again
  const char *ptr = (const char *) buffer;
  for (unsigned i = 0; i < size; i++){
    check_valid_ptr ((const void *) ptr);    
    ptr++; 
  }
}


static struct file* 
get_file_by_fd(int fd)
{
  struct thread *cur = thread_current();
  struct list_elem *e;

  // 找list 看有無開啟的file 有回傳
  for (e = list_begin(&cur->file_descriptor); e != list_end(&cur->file_descriptor); e = list_next(e)){
    struct file_elem *fe = list_entry (e, struct file_elem, elem);
    if(fe->fd == fd){
      return fe->file_ptr;
    }
  }
  // 找不到 回傳null表示不合法
  return NULL;
}


void 
sys_exit(int status) 
{
    struct thread *t = thread_current();
    t->exit_code = status;

    thread_exit ();
}

