#include "panic.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../fs/procfs.h"
#include "../fs/ext2.h"
#include "../syscall/syscall.h"
#include "../proc/init.h"
#include "../../arch/x86_64/gdt.h"
#include "../../arch/x86_64/idt.h"
#include "../../arch/x86_64/pic.h"
#include "../../arch/x86_64/pit.h"
#include "../../drivers/tty.h"
#include "../../drivers/ata.h"
#include "../../drivers/fb.h"
#include "../../drivers/bga.h"
#include "../../drivers/keyboard.h"
#include "../../drivers/serial.h"
#include "../../lib/printk.h"
#include "../../lib/string.h"
#include "../../mm/pmm.h"
#include "../../mm/vmm.h"
#include <stdint.h>
#include <stdbool.h>

static char     cwd[256] = "/";
static uint32_t g_mb2_magic;
static uint64_t g_mb2_info;

extern void cmd_mem(uint32_t, uint64_t);

/* ── path helper ─────────────────────────────────────── */
static void
path_join(const char *base, const char *rel, char *out, size_t max)
{
    if (!rel || !rel[0]) { kstrncpy(out, base, max); return; }
    if (rel[0] == '/') { kstrncpy(out, rel, max); return; }
    kstrncpy(out, base, max);
    size_t len = kstrlen(out);
    if (len > 0 && out[len-1] != '/' && len+1 < max)
        { out[len]='/'; out[len+1]='\0'; len++; }
    kstrncat(out, rel, max - len - 1);
}

/* ── shell commands ──────────────────────────────────── */
static void cmd_ls(const char *path) {
    char full[256]; path_join(cwd, path, full, sizeof(full));
    file_t *f = vfs_open(full, O_RDONLY);
    if (!f) { printk("ls: %s: No such file or directory\n", full); return; }
    if (f->inode->type != FT_DIR) { printk("%s\n", full); vfs_close(f); return; }
    dentry_t d; int n=0;
    while (vfs_readdir(f, &d)) {
        if (!d.name[0]) continue;
        if (d.inode && d.inode->type==FT_DIR) { tty_setcolor_info(); printk("%s/  ",d.name); }
        else { tty_setcolor_reset(); printk("%s  ",d.name); }
        n++;
    }
    if (n) printk("\n");
    tty_setcolor_reset(); vfs_close(f);
}

static void cmd_cat(const char *path) {
    char full[256]; path_join(cwd, path, full, sizeof(full));
    file_t *f = vfs_open(full, O_RDONLY);
    if (!f) { printk("cat: %s: No such file\n", full); return; }
    char buf[256]; int64_t n;
    while ((n=vfs_read(f,buf,sizeof(buf)-1))>0) { buf[n]='\0'; tty_puts(buf); }
    vfs_close(f);
}

static void cmd_mkdir(const char *path) {
    char full[256]; path_join(cwd, path, full, sizeof(full));
    if (vfs_create(full, FT_DIR)<0) printk("mkdir: cannot create '%s'\n", full);
}

static void cmd_touch(const char *path) {
    char full[256]; path_join(cwd, path, full, sizeof(full));
    file_t *f = vfs_open(full, O_WRONLY|O_CREAT);
    if (f) vfs_close(f); else printk("touch: cannot create '%s'\n", full);
}

static void cmd_rm(const char *path) {
    char full[256]; path_join(cwd, path, full, sizeof(full));
    if (vfs_unlink(full)<0) printk("rm: cannot remove '%s'\n", full);
}

static void cmd_write(const char *path, const char *data) {
    char full[256]; path_join(cwd, path, full, sizeof(full));
    file_t *f = vfs_open(full, O_WRONLY|O_CREAT|O_TRUNC);
    if (!f) { printk("write: cannot open '%s'\n", full); return; }
    vfs_write(f, data, kstrlen(data)); vfs_write(f,"\n",1); vfs_close(f);
}

static void cmd_append(const char *path, const char *data) {
    char full[256]; path_join(cwd, path, full, sizeof(full));
    file_t *f = vfs_open(full, O_WRONLY|O_CREAT|O_APPEND);
    if (!f) { printk("append: cannot open '%s'\n", full); return; }
    vfs_write(f, data, kstrlen(data)); vfs_write(f,"\n",1); vfs_close(f);
}

static void cmd_cd(const char *path) {
    if (!path||!path[0]) { kstrcpy(cwd,"/root"); return; }
    if (!kstrcmp(path,"..")) {
        size_t len=kstrlen(cwd); if(len<=1) return;
        char *p=cwd+len-1; if(*p=='/') p--;
        while(p>cwd&&*p!='/') p--;
        if(p==cwd) cwd[1]='\0'; else *p='\0'; return;
    }
    char full[256]; path_join(cwd, path, full, sizeof(full));
    file_t *f = vfs_open(full, O_RDONLY);
    if (!f) { printk("cd: %s: No such directory\n", path); return; }
    if (f->inode->type!=FT_DIR) { printk("cd: %s: Not a directory\n",path); vfs_close(f); return; }
    vfs_close(f);
    kstrncpy(cwd, full, sizeof(cwd));
    size_t len=kstrlen(cwd);
    if (len>1 && cwd[len-1]=='/') cwd[len-1]='\0';
}

static void cmd_cp(const char *s, const char *d) {
    char fs[256],fd[256];
    path_join(cwd,s,fs,sizeof(fs)); path_join(cwd,d,fd,sizeof(fd));
    file_t *fin=vfs_open(fs,O_RDONLY); if(!fin){printk("cp: cannot read '%s'\n",s);return;}
    file_t *fout=vfs_open(fd,O_WRONLY|O_CREAT|O_TRUNC);
    if(!fout){printk("cp: cannot write '%s'\n",d);vfs_close(fin);return;}
    char buf[512]; int64_t n;
    while((n=vfs_read(fin,buf,sizeof(buf)))>0) vfs_write(fout,buf,n);
    vfs_close(fin); vfs_close(fout);
}

static void cmd_mv(const char *s, const char *d) {
    cmd_cp(s,d);
    char full[256]; path_join(cwd,s,full,sizeof(full)); vfs_unlink(full);
}

static void cmd_edit(const char *path) {
    char full[256]; path_join(cwd,path,full,sizeof(full));
    printk("--- edit %s  ('.' to save, ':q' cancel) ---\n", full);
    char content[4096]; content[0]='\0'; char line[256];
    for(;;) {
        tty_setcolor_info(); printk("> "); tty_setcolor_reset();
        kbd_readline(line,sizeof(line));
        if(!kstrcmp(line,".")) break;
        if(!kstrcmp(line,":q")) { printk("Cancelled.\n"); return; }
        size_t cur=kstrlen(content),add=kstrlen(line);
        if(cur+add+2<sizeof(content))
            { kmemcpy(content+cur,line,add); content[cur+add]='\n'; content[cur+add+1]='\0'; }
    }
    file_t *f=vfs_open(full,O_WRONLY|O_CREAT|O_TRUNC);
    if(!f){printk("edit: cannot save\n");return;}
    vfs_write(f,content,kstrlen(content)); vfs_close(f);
    printk("Saved %u bytes.\n",(uint64_t)kstrlen(content));
}

static void cmd_stat(const char *path) {
    char full[256]; path_join(cwd,path,full,sizeof(full));
    inode_t *ino=vfs_lookup(full);
    if(!ino){printk("stat: %s: No such file\n",full);return;}
    static const char *types[]={"unknown","regular","directory","char","block","fifo","link"};
    printk("  File: %s\n  Type: %s\n  Size: %u bytes\n",
           full, ino->type<7?types[ino->type]:"?", ino->size);
}

/* ── shell ───────────────────────────────────────────── */
static void
shell(void)
{
    char line[256];
    for (;;) {
        tty_setcolor_ok();
        printk("root@sinux:%s# ", cwd);
        tty_setcolor_reset();
        kbd_readline(line, sizeof(line));

        char *args[16]; int argc=0; char *p=line;
        while(*p&&argc<16) {
            while(*p==' ')p++;
            if(!*p) break;
            args[argc++]=p;
            while(*p&&*p!=' ')p++;
            if(*p){*p='\0';p++;}
        }
        if(!argc) continue;
        const char *cmd=args[0];

        if (!kstrcmp(cmd,"help")) {
            tty_setcolor_info();
            tty_puts(
                "  ls  [path]          list directory\n"
                "  cd  <path>          change directory (cd .. / cd ~)\n"
                "  pwd                 print working directory\n"
                "  mkdir <path>        create directory\n"
                "  touch <file>        create empty file\n"
                "  rm    <file>        remove file\n"
                "  cat   <file>        print file\n"
                "  edit  <file>        line editor\n"
                "  write <file> <txt>  write text to file\n"
                "  append <file> <txt> append to file\n"
                "  cp <src> <dst>      copy file\n"
                "  mv <src> <dst>      move/rename file\n"
                "  stat  <file>        file info\n"
                "  mem / memstat       memory\n"
                "  uptime / uname      system info\n"
                "  clear               clear screen\n"
                "  halt                shutdown\n"
            );
            tty_setcolor_reset();
        }
        else if(!kstrcmp(cmd,"ls"))     cmd_ls(argc>1?args[1]:cwd);
        else if(!kstrcmp(cmd,"cd"))     cmd_cd(argc>1?args[1]:NULL);
        else if(!kstrcmp(cmd,"pwd"))    printk("%s\n",cwd);
        else if(!kstrcmp(cmd,"mkdir"))  { if(argc>1)cmd_mkdir(args[1]); else printk("mkdir: missing arg\n"); }
        else if(!kstrcmp(cmd,"touch"))  { if(argc>1)cmd_touch(args[1]); else printk("touch: missing arg\n"); }
        else if(!kstrcmp(cmd,"rm"))     { if(argc>1)cmd_rm(args[1]);    else printk("rm: missing arg\n"); }
        else if(!kstrcmp(cmd,"cat"))    { if(argc>1)cmd_cat(args[1]);   else printk("cat: missing arg\n"); }
        else if(!kstrcmp(cmd,"edit"))   { if(argc>1)cmd_edit(args[1]);  else printk("edit: missing arg\n"); }
        else if(!kstrcmp(cmd,"stat"))   { if(argc>1)cmd_stat(args[1]);  else printk("stat: missing arg\n"); }
        else if(!kstrcmp(cmd,"write"))  { if(argc>2)cmd_write(args[1],args[2]); else printk("usage: write <f> <txt>\n"); }
        else if(!kstrcmp(cmd,"append")) { if(argc>2)cmd_append(args[1],args[2]); else printk("usage: append <f> <txt>\n"); }
        else if(!kstrcmp(cmd,"cp"))     { if(argc>2)cmd_cp(args[1],args[2]); else printk("usage: cp <s> <d>\n"); }
        else if(!kstrcmp(cmd,"mv"))     { if(argc>2)cmd_mv(args[1],args[2]); else printk("usage: mv <s> <d>\n"); }
        else if(!kstrcmp(cmd,"mem"))    cmd_mem(g_mb2_magic,g_mb2_info);
        else if(!kstrcmp(cmd,"memstat")) {
            uint64_t t=(uint64_t)pmm_total_pages()*PAGE_SIZE/(1024*1024);
            uint64_t f=(uint64_t)pmm_free_pages() *PAGE_SIZE/(1024*1024);
            printk("  Total:%u MiB  Free:%u MiB  Used:%u MiB\n",t,f,t-f);
        }
        else if(!kstrcmp(cmd,"uptime")) {
            uint64_t ms=pit_uptime_ms();
            printk("  up %u.%03u seconds\n",ms/1000,ms%1000);
        }
        else if(!kstrcmp(cmd,"uname"))  printk("Sinux 1.0.0 MVP\n");
        else if(!kstrcmp(cmd,"clear"))  { tty_clear(); }
        else if(!kstrcmp(cmd,"halt"))   {
            tty_setcolor_err(); tty_puts("Halting...\n");
            __asm__ volatile("cli"); for(;;) __asm__ volatile("hlt");
        }
        else {
            tty_setcolor_err();
            printk("  command not found: %s\n", cmd);
            tty_setcolor_reset();
        }
    }
}

void
kernel_main(uint32_t mb2_magic, uint64_t mb2_info)
{
    g_mb2_magic = mb2_magic;
    g_mb2_info  = mb2_info;

    /*
     * مرحله ۱: فقط serial رو init کن.
     * تا وقتی fb map نشده، هیچ چیزی به framebuffer نمی‌نویسیم.
     * printk از این لحظه فقط روی serial کار می‌کنه.
     */
    serial_init();

    /*
     * مرحله ۲: زیرساخت اصلی CPU و حافظه
     * ترتیب اینجا مهمه — IDT باید قبل از هر چیزی باشه
     * که ممکنه fault بده (از جمله vmm و bga).
     */
    gdt_init();
    idt_init();
    pic_init();
    pit_init();
    keyboard_init();
    pmm_init(mb2_magic, mb2_info);
    vmm_init();

    /*
     * مرحله ۳: گرافیک
     *
     * حالا IDT و VMM آماده‌ان — می‌تونیم BGA رو روشن کنیم.
     *
     * چرا ترتیب اینجا مهمه؟
     *   bga_init() فقط رجیسترهای دستگاه رو ست می‌کنه و آدرس واقعی
     *   framebuffer رو از PCI config space (BAR0) کشف می‌کنه —
     *   خودش هنوز به حافظه چیزی نمی‌نویسه، پس صدا زدنش قبل از
     *   map کردن مشکلی نداره. اما fb_init() اولین کسیه که واقعاً
     *   می‌نویسه، پس باید حتماً بعد از vmm_map_range بیاد.
     *
     * ترتیب صحیح:
     *   1) bga_init       — کارت رو روشن کن، آدرس واقعی رو از PCI بگیر
     *   2) vmm_map_range  — همون آدرس واقعی رو map کن
     *   3) fb_init        — حالا امنه که بنویسیم؛ صفحه رو پاک کن
     *   4) tty_init       — حالا tty می‌تونه از fb استفاده کنه
     */
    if (bga_init(1024, 768, 32) == 0) {
        uint64_t fb_phys = bga_fb_phys_addr();

        vmm_map_range(vmm_kernel_pml4(),
                      fb_phys,         /* virt — همون phys (identity) */
                      fb_phys,         /* phys — آدرس واقعی از PCI BAR0 */
                      1024 * 768 * 4,  /* bytes: width * height * (bpp/8) */
                      VMM_PRESENT | VMM_WRITABLE);

        fb_init();   /* صفحه رو سیاه کن، cursor = (0,0) */
    }

    /* حالا tty می‌تونه از fb_putc استفاده کنه */
    tty_init();

    /*
     * مرحله ۴: بقیه سابسیستم‌ها
     */
    vfs_init();
    ramfs_mount("/");
    procfs_mount("/proc");
    tty_dev_init();
    proc_init();
    sched_init();
    syscall_init();

    ata_init();
    ata_drive_t *disk = ata_get_drive(0);
    if (disk) {
        vfs_create("/mnt",      FT_DIR);
        vfs_create("/mnt/disk", FT_DIR);
        if (ext2_mount("/mnt/disk", disk, 0)==0)
            printk(KERN_INFO "fs: ext2 at /mnt/disk\n");
        else
            printk(KERN_WARNING "fs: no ext2 on disk\n");
    }

    vfs_create("/root", FT_DIR);
    vfs_create("/home", FT_DIR);
    vfs_create("/etc",  FT_DIR);
    vfs_create("/tmp",  FT_DIR);
    vfs_create("/bin",  FT_DIR);
    vfs_create("/lib",  FT_DIR);
    vfs_create("/usr",  FT_DIR);

    kstrcpy(cwd, "/root");

    __asm__ volatile("sti");

    tty_setcolor_info();
    tty_puts("############################################\n"
             "#                  Sinux                  #\n"
             "#    Made By SUN (Sinux Users Network)    #\n"
             "############################################\n");
    tty_setcolor_reset();

    uint64_t fr = (uint64_t)pmm_free_pages()  * PAGE_SIZE / (1024*1024);
    uint64_t to = (uint64_t)pmm_total_pages() * PAGE_SIZE / (1024*1024);
    printk(KERN_INFO "RAM: %u/%u MiB\n\n", fr, to);

    if (proc_spawn_init() == 0) {
        printk(KERN_INFO
               "kernel: idle — init will run on next scheduler tick\n");
        for (;;)
            __asm__ volatile("hlt");
    }

    printk(KERN_WARNING
           "kernel: no init binary — falling back to kernel shell\n"
           "        (place ELF at /sbin/init or /bin/init)\n\n");
    shell();
}
