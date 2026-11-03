# نصب‌کننده سینوکس ۱٫۱٫۰ — راهنمای فارسی

> **⚠ رفع اشکال فوری (آخرین بیلد):** اگر فقط یک خط چشمکزن گوشهٔ بالا سمت چپ
> می‌بینید و نصب‌کننده باز نمیشود ← [`BOOT-HELP.fa.md`](./BOOT-HELP.fa.md).
> علت: `/dev/console` به `ttyS0` (پورت سریال ناموجود) وصل بود؛ حالا گزینه‌های
> پیشفرض `console=tty0` هستند، `quiet` حذف شده و `/init` کنسول واقعی را
> تشخیص می‌دهد. گزینهٔ **Safe text mode (VGA only, no dialog)** هم اضافه شد.

> **⚠ رفع اشکال دوم (آخرین بیلد):** نصب انجام می‌شد ولی دستگاه **ریبوت
> نمیشد** و دوباره منوی نصب‌کننده بالا میآمد؛ اگر هم دستی ریبوت میزدید، فرمور
> باز هم USB را اول بوت میکرد. الان بعد از اتمام نصب پیام «USB را جدا کنید»
> میآید و سیستم **خودکار ریبوت میشود**، منوی GRUB ایزو سیستم نصب‌شده را شناسایی
> و آن را پیشفرض میکند (حتی با USB وصل)، و در UEFI ورودی `BootNext` هم ساخته
> میشود. نتیجه: ترمینال `root@sinux:/root#`. جزئیات: [`BOOT-HELP.fa.md`](./BOOT-HELP.fa.md).

> **نکته مهم:** پروژه‌ای که لینک دادید (`github.com/SinuxProject/Sinux`) **لینوکس نیست**.
> طبق README خودش: «It is not Linux. It does not run Linux programs.» یک کرنل ۶۴‌بیتی مستقل است که از صفر به C و اسمبلی نوشته شده و فقط یک شل ترمینالی داخلی دارد.
> بنابراین ISO تحویل‌شده **یک لینوکس زندهٔ مینیمال فقط به‌عنوان «وسیلهٔ نصب»** است؛ چیزی که روی دیسک نصب می‌شود **کرنل سینوکس** است، نه یک توزیع لینوکس.

---

## ۰. چه باگی در نسخهٔ ۱٫۰ گرفته شد (مهم)

روی سخت‌افزار واقعی، نسخهٔ ۱٫۰ با این پیام **کل ماشین قفل می‌کرد**:

```
---[ end Kernel panic - not syncing: Attempted to kill init! exitcode=0x00000000 ]---
```

`ORIG_RAX: 0xe7` یعنی پردازش شمارهٔ ۱ (`init`) تابع `exit_group(0)` را صدا زده بود.
در لینوکس اگر PID 1 بمیرد، کرنل پنیک می‌کند و چون هیچ راهی برای ادامه نیست،
ظاهراً «کل سخت‌افزار درگیر/freez» می‌شود. علت در کد قدیمی:

- `/init` مستقیماً `exec /usr/local/sbin/sinux-install` می‌کرد، یعنی خود installer همان PID 1 بود.
- مسیرهای متعددی بدون `exec`/حلقهٔ نگهدارنده از انتهای اسکریپت خارج می‌شدند:
  `Cancel` در منو (`ui_menu ... || break`)، پایان `dryrun` بعد از `sleep 2`،
  `exit` در شل rescue، شکست `dialog` روی کنسول واقعی، شکست `poweroff -f`/`reboot -f`،
  و `die()` که `exit 1` می‌کرد.

**راه‌حل ۱٫۱٫۰:** `/init` حالا یک **supervisor** است؛ installer را به‌عنوان
پردازش فرزند اجرا می‌کند و اگر به هر دلیلی خارج شد، لاگ می‌نویسد و یک شل
rescue می‌دهد. PID 1 هیچ‌وقت نمی‌میرد. همچنین `panic=15` اضافه شد تا حتی
در بدترین حالت ماشین برای همیشه قفل نماند. بقیهٔ اصلاحات:

| مورد | اصلاح |
|---|---|
| Cancel / EOF / شکست dialog | سقوط خودکار به منوی متنی؛ منو هیچ‌وقت بسته نمی‌شود |
| پارتیشن‌بندی | اسکریپت `sfdisk` مستقل از اندازهٔ سکتور (۵۱۲ و 4Kn) |
| حفاظت از رسانهٔ نصب | برچسب ISO + برچسب پارتیشن‌ها + حذف `sr*`/`loop*`/دیسک فقط‌خواندنی |
| سیم‌لینک‌های ناقص | زنجیرهٔ کامل در initramfs کپی می‌شود (`awk`، `modprobe` و …) |
| هشدارهای gconv / locale / terminfo | داده‌های لازم اضافه شد |
| تشخیص کنسول فارسی | به‌جای حدس از cmdline، از `/sys/class/tty/console/active` خوانده می‌شود |
| `Invalid ELF header magic` | لیست ماژول‌ها بازنویسی و `sinux.debug=1` برای ردیابی اضافه شد |
| باینری قدیمی `hello.elf` | بیلد `make all` به‌جای فقط `kernel.elf` |
| رابط دانلود | `index.html` صفحهٔ دانلود؛ فایل ISO مستقیماً در ریشهٔ ورکسپیس |

تست‌های اضافه‌شده: خروج از rescue، Cancel مکرر، dry-run، EOF کنسول، شکست
`poweroff`، بوت 4Kn، و بررسی صریح اینکه رشتهٔ `Attempted to kill init` در لاگ
هیچ‌کدام از بوت‌ها وجود ندارد.

---

## ۱. فایل‌های تحویل‌شده (همه در ریشهٔ ورکسپیس)

| فایل | توضیح |
|---|---|
| `Sinux-Installer-1.1.0-x86_64.iso` | **خروجی نهایی** — ISO هایبرید قابل بوت با BIOS و UEFI، ۶۹٬۲۲۶٬۴۹۶ بایت (۶۶ MiB) |
| `SHA256SUMS` | چک‌سام SHA256 ایزو و باندل سورس |
| `sinux-installer-source.tar.gz` | سورس کامل نصب‌کننده (بدون ریپوی گیتهاب؛ آن را جداگانه از GitHub بگیرید) |
| `README.fa.md` | همین راهنما |
| `index.html` | صفحهٔ دانلود |
| `verification-1.1.0.log` | گزارش تست کامل نسخهٔ جدید |
| `BOOT-HELP.fa.md` | رفع مشکل «صفحهٔ سیاه / خط چشمکزن / installer باز نمیشود» |

سورس نصب‌کننده هم در `sinux-installer/` قرار دارد:

```
sinux-installer/
├── build.sh                  اسکریپت ساخت قابل تکرار
├── src/
│   ├── init                  /init اینیش‌رمفس لینوکس زنده
│   ├── sinux-install         اینستالر CLI/TUI دوزبانه (بش + dialog)
│   ├── grub.cfg.iso          منوی GRUB ایزو
│   ├── i18n/en.msg           کاتالوگ انگلیسی
│   ├── i18n/fa.msg           کاتالوگ فارسی (خام)
│   ├── i18n/fa-console.msg   فارسی پیش‌شکل‌داده‌شده برای کنسول VGA
│   ├── make_fa_console.py    تولید کاتالوگ شکل‌داده‌شده
│   ├── collect_libs.py       کپی باینری‌ها و کتابخانه‌ها
│   ├── host-install.sh       رایت امن ISO روی USB از روی یک لینوکس معمولی
│   └── README.txt            توضیحات داخل ISO
└── patches/sinux-build-fixes.patch  پچ‌های لازم برای بیلد کرنل سینوکس
```

---

## ۲. نحوه استفاده

### رایت روی USB (از روی لینوکس میزبان)

```bash
sudo ./sinux-installer/src/host-install.sh Sinux-Installer-1.1.0-x86_64.iso
```

یا دستی (مراقب باشید نام درست باشد):

```bash
sudo dd if=Sinux-Installer-1.1.0-x86_64.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

### بوت

1. وارد BIOS/UEFI شوید، **Secure Boot را خاموش کنید**.
2. از USB (یا در ماشین مجازی از ISO) بوت کنید.
3. منوی GRUB نمایش داده می‌شود:

```
Sinux Installer 1.1.0 - English (text mode, BIOS + UEFI)       <-- پیشفرض
Sinux Installer 1.1.0 - Farsi / Persian (text mode, BIOS + UEFI) <-- زبان فارسی
Sinux Installer 1.1.0 - Safe text mode (VGA only, no dialog)     <-- اگر گیر کرد
Sinux Installer 1.1.0 - Serial console 115200 (English)
Sinux Installer 1.1.0 - Verbose / debug (serial, all messages)
Sinux Installer 1.1.0 - Rescue shell (no installer)
Sinux Installer 1.1.0 - Dry run (show plan, change nothing)
Boot Sinux kernel directly (live Multiboot2, no install)
Boot installed Sinux from this disk (system terminal)  <-- بعد از نصب، پیشفرض
```

زبان را همین‌جا انتخاب می‌کنید؛ داخل منوی اصلی هم گزینهٔ «تغییر زبان» وجود دارد.

### مراحل اینستالر

1. صفحهٔ خوش‌آمد و هشدار «پاک شدن کامل دیسک».
2. انتخاب دیسک هدف (خود رسانهٔ نصب از لیست حذف می‌شود با استفاده از Volume Label برچسب `SINUX_INST`).
3. **تایپ کردن نام دقیق دیسک** برای تأیید (مثلاً `sda` یا `nvme0n1`) — بدون این تأیید هیچ چیزی پاک نمی‌شود.
4. انتخاب فایل‌سیستم ریشه: `ext2` (پیشنهادی) یا `ext4`.
5. وارد کردن hostname.
6. نمایش خلاصهٔ برنامه و تأیید نهایی.
7. اجرا: پارتیشن‌بندی ← فرمت ← کپی کرنل ← نصب GRUB برای BIOS و UEFI ← نوشتن `grub.cfg`.
8. پیام «نصب کامل شد» و پیشنهاد ریبوت.

منوی **Dry run** هم هست که فقط برنامه را نشان می‌دهد و هیچ تغییری روی دیسک نمی‌دهد.

### چیدمان دیسک (GPT، سازگار با هر دو حالت بوت)

| پارتیشن | اندازه | نوع | کاربرد |
|---|---|---|---|
| p1 | ۱ MiB | BIOS Boot (`21686148-…`) | core.img GRUB برای BIOS قدیمی |
| p2 | ۱۲۸ MiB | EFI System (FAT32) | `/boot/efi` + `EFI/BOOT/BOOTX64.EFI` |
| p3 | باقی‌مانده | Linux filesystem | ریشه سینوکس، پیش‌فرض `ext2` |

فایل‌های نصب‌شده روی p3:

```
/boot/kernel.elf            کرنل سینوکس (بیلدشده از سورس ریپو)
/boot/grub/grub.cfg         منوی بوت با multiboot2
/bin/hello                  نمونهٔ برنامهٔ userspace ریپو
/etc/sinux-release
/etc/hostname, /etc/issue, /etc/motd
/etc/sinux/install-info.json
/etc/sinux/install.log      لاگ کامل نصب
/usr/share/doc/sinux/*
```

---

## ۳. پارامترهای خط فرمان کرنل (در منوی GRUB با `e` ویرایش کنید)

```
sinux.lang=en|fa
sinux.mode=install|auto|dryrun|rescue
sinux.device=/dev/vda       فقط در حالت auto
sinux.fs=ext2|ext4
sinux.hostname=NAME
sinux.yes=1                 رد کردن تأییدها
sinux.reboot=0|1            پایان کار در حالت auto
sinux.debug=1               لاگ کامل modprobe و پیامهای کرنل
sinux.nodialog=1            اجبار رابط متنی ساده
sinux.term=TERM             تغییر TERM (مثلاً vt100)
sinux.fa_console=auto|shaped|raw
```

مثال نصب خودکار (مناسب CI / QEMU):

```
sinux.mode=auto sinux.device=/dev/vda sinux.yes=1 sinux.reboot=0
```

---

## ۴. تست‌های انجام‌شده (همه روی QEMU 10 / Debian 13)

> **نتیجهٔ نهایی نسخهٔ ۱٫۱٫۰: ۷۴ تست، ۷۴ پاس، ۰ شکست، ۰ پنیک.**
> شمارش دقیق و لاگ هر تست: `verification-1.1.0.log`.
> گزارش کامل: `verification-1.1.0.log` — اسکریپت قابل اجرای مجدد:
> `./sinux-installer/tests/qemu-tests.sh` (پس از `./build.sh`).

| تست | نتیجه |
|---|---|
| بوت ISO در حالت BIOS و نمایش منوی GRUB | ✅ |
| بوت لینوکس زنده و بالا آمدن رابط `dialog` (انگلیسی + فارسی) | ✅ |
| بوت مستقیم کرنل سینوکس (Multiboot2) تا شل `root@sinux:/root#` | ✅ (۲۱/۲۱ تست داخلی پاس) |
| نصب خودکار کامل روی دیسک ۵۱۲ MiB در حالت `auto` | ✅ بدون خطا |
| بررسی جدول GPT و فایل‌ها از روی هاست | ✅ ۳ پارتیشن، `kernel.elf`، `grub.cfg`، لاگ، `install-info.json` |
| `EFI/BOOT/BOOTX64.EFI` روی ESP | ✅ ۶۴۷٬۱۶۸ بایت |
| بوت دیسک نصب‌شده در **BIOS** | ✅ تا شل سینوکس |
| بوت دیسک نصب‌شده در **UEFI / OVMF** | ✅ تا شل سینوکس |
| امضای MBR توسط GRUB i386-pc | ✅ |
| **عدم پنیک `Attempted to kill init` در تمام ۱۶ سناریو** | ✅ |
| خروج از شل rescue (همان مسیر کرش ۱٫۰) → supervisor شل جدید می‌دهد | ✅ |
| نصب فارسی خودکار روی ماشین ۲۵۶ MiB | ✅ |
| initramfs XZ + hardlink کتابخانه‌ها (۶۹ MiB ISO) | ✅ |
| بوت ISO در UEFI / OVMF با منوی ۹ گزینه‌ای | ✅ |
| **کنسول فقط VGA (بدون پورت سریال) → نصبکننده اجرا شده و دیسک نوشته میشود** | ✅ |
| **اثبات بصری روی فریمبافر VGA: منوی GRUB و پنجرهٔ نصبکننده واقعاً رسم میشوند** | ✅ |
| **USB همچنان وصل باشد → منوی GRUB، سیستم نصبشده را بوت میکند** (`root@sinux:/root#`) | ✅ |
| **نصب تعاملی بعد از اتمام ریبوت میکند، نه برگشت به منو** | ✅ |
| **UEFI: ورودی NVRAM و `BootNext` برای دیسک نصبشده** | ✅ |
| initramfs XZ روی ماشین ۲۵۶ MiB (بدون `Initramfs unpacking failed`) | ✅ |
| تست باینری‌های initramfs داخل chroot (`bash`, `awk`, `dialog`, `grub-install`, …) | ✅ |
| پارتیشن‌بندی روی دیسک 4Kn (سکتور ۴۰۹۶) | ✅ |

---

## ۵. ساخت مجدد از سورس

پیش‌نیازها (Debian 13 x86_64):

```bash
sudo apt install -y --no-install-recommends \
  nasm xorriso mtools dosfstools e2fsprogs fdisk util-linux kmod dialog bash kbd \
  unifont console-setup-linux busybox-static parted efibootmgr cpio xz-utils \
  grub-pc-bin grub-efi-amd64-bin grub-common grub2-common \
  qemu-system-x86 ovmf python3-venv git make gcc binutils
```

> **ریپوی گیتهاب حذف شد:** تمام فایلهایی که از `github.com/SinuxProject/Sinux`
> قابل دانلود هستند از ورکسپیس پاک شدند تا فقط فایلهای مهم بمانند. برای ساخت
> مجدد، همان ریپو را دوباره بگیرید (`build.sh` در نبودش راهنمایی چاپ میکند):

```bash
git clone --depth 1 https://github.com/SinuxProject/Sinux.git Sinux
```

سپس:

```bash
cd sinux-installer
./build.sh            # خروجی در release/ (پوشه ساخته میشود)
./build.sh --clean    # مثل قبل + حذف کش سنگین (فایل .deb و ماژول‌ها)
```

`build.sh` خودش کرنل دبیان را دانلود و استخراج می‌کند، کرنل سینوکس را می‌سازد،
initramfs می‌سازد و در نهایت `grub-mkrescue` را صدا می‌زند.

برای تولید دوبارهٔ کاتالوگ فارسی شکل‌داده‌شده:

```bash
python3 -m venv /tmp/v && /tmp/v/bin/pip install arabic-reshaper python-bidi
cd sinux-installer/src/i18n
/tmp/v/bin/python ../make_fa_console.py fa.msg fa-console.msg
```

> چرا «پیش‌شکل‌داده»؟ کنسول متنی لینوکس موتور bidi/shaping ندارد. پس متن‌های فارسی
> برای نمایش روی VGA با `arabic-reshaper` + `python-bidi` به فرم‌های ارائهٔ عربی و ترتیب
> دیداری تبدیل می‌شوند و با فونت `Arabic-VGA16.psf` نمایش داده می‌شوند.
> روی کنسول سریال، متن خام فارسی فرستاده می‌شود تا ترمینال خودتان bidi را انجام دهد.

---

## ۶. پچ‌هایی که روی ریپوی سینوکس اعمال شد

ریپوی `main` (آخرین کامیت `f95d6a0`) همان‌طور که هست بیلد نمی‌شد. پچ‌ها در
`patches/sinux-build-fixes.patch` هستند و شامل:

1. `Makefile`: هدف پیش‌فرض اشتباه بود (فقط `boot.o` ساخته می‌شد) ← اضافه شدن `.DEFAULT_GOAL := all` و حذف آبجکت‌های تکراری با `$(sort ...)`.
2. `arch/x86_64/smp/cpu.c` و `lock.c`: مسیرهای include اشتباه (`../../lib` به جای `../../../lib`).
3. `arch/x86_64/smp/cpu.c`: اسمبلی inline با `.code16/.code32` اسمبلر را در حالت ۳۲ بیت رها می‌کرد ← اضافه شدن `.code64`.
4. `arch/x86_64/smp/cpu.h`: تعریف `IPI_VECTOR_TLB_FLUSH` که `lock.c` استفاده می‌کرد.
5. `userspace/libc/Makefile` و `hello/Makefile`: اضافه شدن مسیر هدرهای freestanding gcc برای `stdarg.h`.
6. `arch/x86_64/linker.ld`: `ASSERT` داخل `SECTIONS` خطای نحوی می‌داد و بخش `.trampoline` بعد از ۱ MiB قرار داشت (relocation ۱۶ بیتی ترانک می‌شد) ← انتقال به `0x7000` قبل از `. = 1M`.
7. حذف `kernel/scheduler/sched.o` تکراری (تعاریف چندگانه `sched_tick` و غیره).
8. `kernel/core/main.c`: غیرفعال کردن `smp_init()/smp_bringup_aps()/sched_preempt_init()` چون خود README upstream می‌گوید «Single core only (no SMP)» و trampoline هنوز قابل جابه‌جایی نیست.

با این پچ‌ها `make all` موفق شد و کرنل ۸۱٬۳۵۲ بایتی در QEMU تا شل بالا آمد.

---

## ۷. محدودیت‌ها (مهم)

- **Secure Boot باید خاموش باشد** (GRUB امضا نشده است).
- **حافظهٔ زندهٔ نصب‌کننده:** initramfs با XZ فشرده شده (۳۵ MiB به‌جای ۵۵ MiB با gzip)
  تا روی ماشین‌های ۲۵۶ MiB هم باز شود. با نسخهٔ ۱٫۰ (gzip) کرنل با
  `Initramfs unpacking failed: write error` پنیک می‌کرد. حداقل پیشنهادی ۲۵۶ MiB است.
- **محدودیت PMM کرنل سینوکس در حالت BIOS:** کرنل upstream در حالت BIOS با RAM
  ≥ ۵۱۲ MiB هنگ می‌کند (تا ۳۸۴ MiB سالم است). در UEFI/OVMF با ۵۱۲ MiB تست شده و سالم است.
  این محدودیت خود کرنل سینوکس است، نه نصب‌کننده؛ README upstream هم ۶۴ MiB حداقل و
  ۲۵۶ MiB پیشنهادی اعلام کرده است.
- کرنل سینوکس (طبق README upstream): تک‌هسته، بدون شبکه، بدون درایور USB (کیبورد PS/2)،
  درایور دیسک فقط ATA PIO، پشتیبانی ext2 ساده، بدون لینک دینامیک، بدون swap.
- **نکتهٔ سازگاری ذخیره‌سازی:** درایور ext2 سینوکس انتظار دارد **کل دیسک ATA خام ext2 باشد**
  (همان `sinux.img`ای که `make iso` می‌سازد) و جدول پارتیشن نمی‌شناسد. چون این نصب‌کننده
  برای بوت BIOS+UEFI از GPT استفاده می‌کند، هنگام بوت پیام
  `[ERR] ext2: bad magic` / `[WARN] fs: no ext2 on disk` می‌بینید و `/mnt/disk` اتو‌مانت
  نمی‌شود. **سیستم همچنان کامل بوت می‌شود و شل کار می‌کند**؛ فقط ذخیره‌سازی پایدار داخلی
  روی همان دیسک در دسترس نیست. اگر ذخیره‌سازی پایدار می‌خواهید، از ISO لایو کرنل سینوکس
  به همراه یک `sinux.img` خام ext2 (بدون جدول پارتیشن) استفاده کنید، یا بعداً درایور پارتیشن
  به کرنل اضافه کنید.
- گزینهٔ `ext4` فقط برای این است که GRUB بتواند بوت کند؛ کرنل سینوکس ext4 نمی‌خواند.
  پیش‌فرض و پیشنهادی `ext2` است.
- نصب‌کننده هرگز بدون تایپ نام دیسک (یا `sinux.yes=1` در حالت auto) دیسکی را پاک نمی‌کند.

---

## ۸. مجوز

- پچ‌ها و کد نصب‌کننده: همان مجوز پروژه، Apache-2.0 (مطابق `LICENSE` ریپو).
- کرنل لینوکس دبیان و بسته‌های کاربری داخل initramfs تابع مجوزهای مربوط به خودشان هستند.
