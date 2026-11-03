# فرستادن تغییرات به گیتهاب / ساختن Pull Request

> **وضعیت کانکتور:** GitHub رو از طریق Arena چک کردم → `unsupported`.
> یعنی نمیتونم با «وصل کردن اکانت» وارد اکانتت بشم. پس یا خودت با `gh`
> اجرا میکنی (**پیشنهاد من**)، یا یه توکن خیلی محدود به من بدی تا از سندباکس
> بزنم. توی هر دو حالت باندل آماده است: **`sinux-installer.bundle`** (۷۲ کیلوبایت،
> شامل تمام سورس، تستها، داکیومنتها و اسکرینشاتها، با تاریخچهٔ git کامل).

---

## ۰. لینکهای مستقیم (همینها رو باز کن)

| لینک | برای چی |
|---|---|
| **https://github.com/settings/personal-access-tokens/new** | 🔗 **صفحهٔ ساخت توکن fine-grained** — مستقیم همینجا برو |
| https://github.com/settings/personal-access-tokens | لیست توکنها / **Revoke** بعد از کار |
| https://github.com/settings/tokens | توکن classic — فقط اگه PR به ریپوی دیگران (مثل `SinuxProject/Sinux`) میخوای |
| **https://github.com/settings/tokens/new?scopes=public_repo&description=Sinux+Installer+PR** | 🔗 لینک پیشپرشدهٔ classic با اسکوپ PR (تیک اسکوپها خودکار زده میشه) |
| https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens | مستندات رسمی گیتهاب |
| https://cli.github.com/ | نصب `gh` (راه امن، بدون توکن دستی) |
| https://docs.github.com/en/rest | اگه منظورت خود REST API بود (`https://api.github.com`) |

**لینک با حداکثر فیلدهای قابلپرشدن** (نام، توضیح، مالک، انقضای ۱ روز،
`Contents: write`، `Pull requests: write`، `Metadata: read`) —
`USERNAME` رو با آیدی گیتهابت عوض کن:

```
https://github.com/settings/personal-access-tokens/new?name=Sinux+Installer+push&description=Push+installer+repo+and+create+PR&target_name=USERNAME&expires_in=1&contents=write&pull_requests=write&metadata=read
```

> **محدودیت رسمی:** گیتهاب توی URL فقط این پارامترها رو میپذیرد:
> `name`، `description`، `target_name`، `expires_in` و `<permission>`
> (یعنی لیست دسترسیها). **هیچ پارامتری برای پر کردن لیست
> «Only select repositories» وجود ندارد** [1](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens#supported-query-parameters).
> پس اون یک لیست حتماً باید دستی انتخاب بشه. اگه نمیخوای هیچ لیستی پر کنی،
> یا توی فرم `All repositories` رو بزن (یک کلیک، ولی دسترسی گستردهتر)، یا
> اصلاً **`gh auth login`** بزن که کل فرم توکن رو حذف میکنه (امنترین راه).
> `Metadata: read` همیشه بهصورت خودکار اضافه میشه.

> گیتهاب رسماً توکن fine-grained رو به classic ترجیح میده چون هم به ریپوی خاص
> محدود میشه هم دسترسیها تکبهتک انتخاب میشن [1](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens).

---

## ۱. اون «API» چیه و از کجا بگیرم

چیزی که لازم داری **Fine-grained Personal Access Token** گیتهابه، نه API key جدا:

1. github.com → عکس پروفایل → **Settings**
2. چپ، پایین → **Developer settings** → **Personal access tokens** → **Fine-grained tokens**
3. **Generate new token**:
   - **Resource owner**: اکانت خودت
   - **Repository access**: `Only select repositories` → فقط ریپوی مقصد/فورک
   - **Permissions**:
     | Permission | سطح | برای چی |
     |---|---|---|
     | `Contents` | Read & Write | push کردن کد |
     | `Pull requests` | Read & Write | ساختن PR روی ریپوهای **خودت** |
     | `Metadata` | Read | اجباری، خودکار اضافه میشه |
     | `Repository creation` | Write | **فقط** اگه میخوای ریپوی جدید بسازی یا فورک کنی |

> ⚠️ **محدودیت توکن fine-grained (تست شد):** بدون `Repository creation: write`،
> `POST /user/repos` و `POST /repos/{owner}/{repo}/forks` با خطای
> `403 Resource not accessible by personal access token` رد میشوند. همچنین
> **نمیشه با توکن fine-grained به ریپوی متعلق به دیگران (مثلاً `SinuxProject/Sinux`)
> PR زد** — همان 403. برای PR به upstream باید یا فورک + `Compare & pull request`
> توی مرورگر بزنی، یا توکن classic با `public_repo` بسازی [3](https://github.com/cheat2001/angkorgit/issues/77).
   - **Expiration**: ۱ روز (کوتاهترین)
4. بعد از اتمام کار حتماً **Revoke** کن.

⚠️ **نکن**: توکن کلاسیک با اسکوپ `repo` نساز (دسترسی به همهٔ ریپوهات میده)،
و ترجیحاً توی چت هم نذار — توی تاریخچهٔ گفتگو ذخیره میشه. امنترین مسیر
همان **راه A** پایین است که اصلاً توکنی ردوبدل نمیشه (`gh auth login` با مرورگر
لاگین میکنه).

---

## ۲. راه A (پیشنهادی) — ریپوی جدید نصبکننده، بدون دادن توکن به من

روی سیستم خودت، جایی که `sinux-installer.bundle` رو دانلود کردی:

```bash
# ۱. باندل رو به یک ریپوی معمولی تبدیل کن
git clone sinux-installer.bundle Sinux-Installer
cd Sinux-Installer

# ۲. هویت واقعی خودت + امضای DCO (کامیت فعلی با هویت موقت import شده)
git config user.name  "Your Real Name"
git config user.email "you@example.com"
git commit --amend --reset-author --signoff --no-edit
git branch -m main

# ۳. لاگین امن با مرورگر (هیچ توکنی توی ترمینال/چت نیست)
gh auth login          # → GitHub.com → HTTPS → Login with a web browser

# ۴. ساخت ریپو و push یکجا
gh repo create Sinux-Installer --public --source=. --remote=origin --push \
  --description "Terminal-only live installer for the Sinux kernel (BIOS+UEFI, EN/FA)"

# ۵. ISO رو بهصورت Release asset بذار (توی git نذار، ۶۶ مگابایته)
gh release create v1.1.0 \
  /path/to/Sinux-Installer-1.1.0-x86_64.iso \
  --title "Sinux Installer 1.1.0" \
  --notes "74/0 QEMU suite. Fixes: invisible VGA console, post-install reboot handover."
```

همچنین فایلهای `README.fa.md`، `BOOT-HELP.fa.md`، `verification-1.1.0.log`،
`index.html` و `SHA256SUMS` رو هم میتونی کنار `README.md` توی ریپو بذاری.
(فعلاً `README.md` و باقی موارد لازم داخل باندل هست.)

---

## ۳. راه B — PR به ریپوی upstream `SinuxProject/Sinux`

> فقط **پچهای بیلد کرنل** به اون ریپو مربوطن
> (`patches/sinux-build-fixes.patch`، ۵ فایل / ۸ اصلاح). نصبکننده یه پروژهٔ
> جداست و نباید مستقیم توی ریپوی کرنل PR بشه.

```bash
gh repo fork SinuxProject/Sinux --clone
cd Sinux
git checkout -b fix/build-and-boot-path
git apply /path/to/Sinux-Installer/patches/sinux-build-fixes.patch
git add -A
git commit -s -m "x86_64: fix linker script, default make goal and single-core boot path"
git push -u origin fix/build-and-boot-path
gh pr create --repo SinuxProject/Sinux --base main \
  --head "YOUR_USER:fix/build-and-boot-path" \
  --title "x86_64: make main build and boot again" \
  --body-file PR-BODY.md
```

متن آمادهٔ `PR-BODY.md`:

```markdown
## What this fixes

`main` at f95d6a0 did not build or boot as-is:

1. `Makefile` had no `.DEFAULT_GOAL`, so a plain `make` produced only `boot.o`.
2. `arch/x86_64/smp/{cpu,lock}.c` used wrong relative include paths.
3. `cpu.c` inline asm left the assembler in 32-bit mode (missing `.code64`).
4. `cpu.h` did not define `IPI_VECTOR_TLB_FLUSH` used by `lock.c`.
5. userspace Makefiles could not find freestanding `stdarg.h`.
6. `linker.ld` had an `ASSERT` inside `SECTIONS` and placed `.trampoline`
   above 1 MiB, truncating 16-bit relocations.
7. duplicate `sched.o` caused multiple definitions.
8. `smp_init()` / AP bring-up is disabled for now, matching upstream's own
   "Single core only (no SMP)" note.

## Testing

Fresh `make all -j4` produces `kernel.elf`, `libc.a` and `hello.elf`.
QEMU boots to `root@sinux:/root#` with 21/21 kernel self-tests passing.

Signed-off-by: Your Real Name <you@example.com>
```

> حتماً `Signed-off-by` رو با نام/ایمیل واقعی خودت عوض کن (DCO).

---

## ۴. راه C — اگه میخوای من از همین سندباکس بزنم

توکن fine-grained بالا رو بساز، ولی قبلش بدون:

- توی چت قرار میگیره و توی تاریخچه میمونه → **بعدش حتماً Revoke کن**.
- من اون رو فقط توی `/tmp` میذارم (جزو فایلهای ذخیرهشدهٔ ورکسپیس نیست)،
  فقط برای یک `git push` + `gh pr create` استفاده میکنم، و بعدش پاک میکنم.
- حداقل دسترسی لازم:
  - push به فورک/ریپوی خودت → `Contents: R/W` روی همان ریپو
  - ساختن PR → `Pull requests: R/W` روی همان ریپو
- اگه PR به `SinuxProject/Sinux` میخوای، نیازی به دسترسی نوشتن روی upstream
  نیست؛ فقط فورک باید توی اکانت خودت باشه.

بعد از اینکه توکن رو دادی، این کارها رو انجام میدم: کلون باندل → تنظیم هویت
و `Signed-off-by` با اطلاعاتی که میدی → push به شاخه → `gh pr create` →
گزارش لینک PR.

---

## ۵. چی توی باندل هست / چی نیست

هست: `build.sh`، `src/` (init، sinux-install، grub.cfg.iso، i18n،
collect_libs.py، make_fa_console.py، host-install.sh، README.txt)،
`tests/` (T1–T16 + vga-proof.py + interactive-install.py)،
`patches/sinux-build-fixes.patch`، `README.md`، `BOOT-HELP.fa.md`،
`verification-1.1.0.log`، `evidence/*.png`.

نیست (عمداً): فایل ISO (۶۶ مگابایت — جاش توی GitHub Release است)،
کلون ریپوی `SinuxProject/Sinux` (از خود گیتهاب قابل دانلوده)، کش بیلد `.build/`.

ISO نهایی:
`aa416b0e2569d4275f1778c8dac05bb3df04bc2be4725e2f2ae9837d5eb78535`
