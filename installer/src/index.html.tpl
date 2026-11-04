<!doctype html>
<html lang="fa" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>دانلود Sinux Installer @@VERSION@@</title>
<style>
  * { box-sizing: border-box; }
  body { margin:0; padding:28px 18px 64px; background:#0d1117; color:#e6edf3;
         font-family: system-ui, -apple-system, "Segoe UI", Tahoma, sans-serif; line-height:1.85; }
  .wrap { max-width:840px; margin:0 auto; }
  h1 { font-size:26px; margin:0 0 4px; color:#58a6ff; }
  .sub { color:#8b949e; margin:0 0 22px; font-size:15px; }
  .card { background:#161b22; border:1px solid #30363d; border-radius:12px;
          padding:22px; margin:16px 0; }
  .btn { display:block; text-align:center; background:#238636; color:#fff;
         text-decoration:none; padding:20px 26px; border-radius:10px;
         font-size:22px; font-weight:800; margin:14px 0; }
  .btn:hover { background:#2ea043; }
  code, .mono { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
                direction:ltr; text-align:left; unicode-bidi:isolate; }
  .hash { display:block; background:#0d1117; border:1px solid #30363d; border-radius:6px;
          padding:10px 12px; font-size:12px; word-break:break-all; color:#79c0ff; margin-top:12px; }
  pre { background:#0d1117; border:1px solid #30363d; border-radius:6px; padding:12px;
        overflow-x:auto; direction:ltr; text-align:left; font-size:13px; color:#d2a8ff; }
  ul { padding-inline-start:22px; }
  a { color:#58a6ff; }
  .tag { display:inline-block; background:#1f6feb22; border:1px solid #1f6feb66;
         color:#79c0ff; border-radius:999px; padding:2px 10px; font-size:12px; }
  .hot { border-color:#d29922; background:#231a0d; }
  .hot h2 { color:#e3b341; }
  .note { color:#8b949e; font-size:13px; }
  table { width:100%; border-collapse:collapse; font-size:14px; }
  th,td { border-bottom:1px solid #30363d; padding:8px 6px; text-align:right; }
</style>
</head>
<body>
<div class="wrap">
  <h1>Sinux Installer @@VERSION@@</h1>
  <p class="sub">نسخهٔ بهترشده · بوت BIOS + UEFI · انگلیسی و فارسی · فقط ترمینال</p>

  <div class="card">
    <span class="tag">ISO 9660 · bootable · SINUX_INST</span>
    <h2 style="margin:14px 0 4px; font-size:20px">فایل دانلود</h2>
    <p class="note" style="margin:0">همین فایل در ریشهٔ ورکسپیس است — نام دقیق:
      <span class="mono">@@ISO@@</span> · @@SIZE@@ بایت</p>
    <a class="btn" href="@@ISO@@" download>⬇ دانلود ISO</a>
    <span class="hash mono">SHA256 @@SHA256@@</span>
    <p style="margin:16px 0 0">رایت روی USB:</p>
    <pre>sudo dd if=@@ISO@@ of=/dev/sdX bs=4M conv=fsync status=progress</pre>
    <p class="note" style="margin:0">حتماً <strong>Secure Boot خاموش</strong> باشد. بعد از پیام
      <span class="mono">Loading ... please wait</span> تا حدود ۶۰ ثانیه صبر کنید.</p>
  </div>

  <div class="card hot">
    <h2 style="margin:0 0 8px; font-size:20px">⚠ فقط یک خط چشمکزن بالای صفحه میبینید؟ <span class="tag">رفع شد</span></h2>
    <p>علت: <span class="mono">console=tty0 console=ttyS0,115200</span> باعث میشد
      <span class="mono">/dev/console</span> به آخرین کنسول (پورت سریال ناموجود) وصل شود و کل رابط
      نصب جایی نوشته شود که کسی نمیبیند. در این نسخه پیشفرضها فقط
      <span class="mono">console=tty0</span> هستند، <span class="mono">quiet</span> حذف شده،
      <span class="mono">/init</span> کنسول واقعی را تشخیص میدهد و گزینهٔ
      <strong>Safe text mode (VGA only, no dialog)</strong> اضافه شده است.</p>
    <p style="margin-bottom:0"><a href="BOOT-HELP.fa.md">📖 راهنمای قدمبهدقم عیبیابی بوت ←</a></p>
  </div>

  <div class="card hot">
    <h2 style="margin:0 0 8px; font-size:20px">⚠ بعد از نصب دوباره منوی نصبکننده میآمد؟ <span class="tag">رفع شد</span></h2>
    <p>دو مشکل بود: نصب تعاملی بعد از اتمام <strong>ریبوت نمیکرد</strong> و مستقیم برمیگشت سر منوی نصبکننده؛ و وقتی دستی ریبوت میزدید، فرمور همچنان USB را اول بوت میکرد. الان بعد از نصب پیام «USB را جدا کنید» میآید و دستگاه <strong>خودکار ریبوت میشود</strong>. تازه منوی GRUB ایزو هم فایل نشانگر سیستم نصبشده را پیدا میکند و گزینهٔ <span class="mono">Boot installed Sinux</span> را پیشفرض قرار میدهد — یعنی حتی اگر USB را نکشید، <strong>ترمینال <span class="mono">root@sinux:/root#</span></strong> بالا میآید. در UEFI یک ورودی NVRAM با <span class="mono">BootNext</span> هم ساخته میشود.</p>
  </div>

  <div class="card">
    <h2 style="margin:0 0 8px; font-size:20px">مدرک بصری <span class="tag">همین ISO</span></h2>
    <p>اسکرینشات خام فریمبافر VGA گرفتهشده با QEMU/QMP، بدون هیچ پورت سریالی:
      منوی ۹ گزینهای GRUB و پنجرهٔ نصبکننده واقعاً رسم شدهاند.</p>
    <ul>
      <li><a href="evidence/01-grub-menu-vga.png">01-grub-menu-vga.png — منوی GRUB</a></li>
      <li><a href="evidence/02-installer-ui-vga.png">02-installer-ui-vga.png — صفحهٔ خوشآمد نصبکننده</a></li>
    </ul>
    <p class="note" style="margin:0">سوییت کامل QEMU: <strong>۷۴ تست / ۷۴ پاس / ۰ شکست / ۰ پنیک (T1–T16)</strong></p>
  </div>

  <div class="card">
    <h2 style="margin:0 0 8px; font-size:20px">فایلهای همراه</h2>
    <table>
      <tr><th>فایل</th><th>توضیح</th></tr>
      <tr><td class="mono">README.fa.md</td><td>راهنمای کامل فارسی</td></tr>
      <tr><td class="mono">BOOT-HELP.fa.md</td><td>رفع مشکل خط چشمکزن / installer باز نمیشود</td></tr>
      <tr><td class="mono">SHA256SUMS</td><td>چکامهٔ تمام فایلها</td></tr>
      <tr><td class="mono">verification-1.1.0.log</td><td>گزارش تست T1–T13</td></tr>
      <tr><td class="mono">sinux-installer-source.tar.gz</td><td>سورس کامل نصبکننده (بدون ریپوی گیتهاب)</td></tr>
      <tr><td class="mono">sinux-installer/</td><td>همان سورس بهصورت پوشهٔ کاری</td></tr>
      <tr><td class="mono">GITHUB-PUSH.fa.md</td><td>گرفتن توکن، push و ساختن Pull Request</td></tr>
      <tr><td class="mono">sinux-installer.bundle</td><td>باندل git آمادهٔ clone (سورس + تستها)</td></tr>
    </table>
  </div>

  <p class="note">این یک لینوکس زندهٔ مینیمال فقط بهعنوان <strong>وسیلهٔ نصب</strong> است؛
    چیزی که روی دیسک نصب میشود <strong>کرنل سینوکس</strong> است (کرنل مستقل از صفر، نه لینوکس).</p>
</div>
</body>
</html>
