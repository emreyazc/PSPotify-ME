# PSPotify BG — PSP'de oyun içi müzik PRX prototipi

**Durum (25 Ekim 2026, yeni sürüm geliştirme aşamasında):** PSP-3000 / 6.61 ARK-5 üzerinde bir PSP oyunu çalışırken **deneme WAV sesi duyuldu**: GAME PRX yüklenmesi ve yerel ses çıkışı doğrulandı. Kullanıcı ilk test melodisinin sesini kalitesiz buldu; bunun üretilmiş örnek ses mi yoksa ses çıkışı kaynaklı mı olduğu henüz ayrıştırılmadı. **Oyunun kendi sesiyle PRX müziğinin aynı anda duyulduğu doğrulandı (MIX-01 PASS).** XMB'ye temiz dönüş, çoklu oyun ve bağımsız müzik ses ayarı henüz ayrıca doğrulanmadı. **Spotify akışı hâlâ PRX'e bağlı değil.**

Bu paketi kullanırken mevcut PSPotify ME `EBOOT.PBP` dosyasına **dokunmuyorsun**. PSP hafızasının `flash0` / `flash1` alanlarına **hiçbir şey yazılmaz**. Eklenti yalnızca bilinçli olarak `PLUGINS.TXT` dosyasına eklenince yüklenir.

## BG-11 — Menü açıkken oyun görüntüsü + sesi duraklatma (deneysel, cihaz testi bekliyor)

**İstek:** Yeni menüde görüntü sahne geçişlerinde yanıp sönüyor / yarım kalıyor. Kullanıcı menü açıkken arkadaki oyunun ses ve görüntü olarak tamamen donmasını istiyor.

**Yeni çözüm:** `bg_pause.c`, PSP Kernel ThreadMan API ile GAME programının kullanıcı iş parçacıklarını tespit eder (`SceKernelThreadInfo.attr`: `PSP_THREAD_ATTR_USER`), **yalnızca daha önceden askıya alınmamış** kullanıcı iş parçacıklarını `sceKernelSuspendThread` ile askıya alır. Kimliklerini saklar, menü kapanınca sadece bunları `sceKernelResumeThread` ile geri başlatır. Başka plugin veya sistem tarafından askıda bulunan thread'lere dokunmaz. Kernel threadleri (PSPotify BG worker, menu renderer, ağ/ses çekirdek sürücüleri) askıya alınmaz. Askıya alma başarısız olursa bu denemedeki başarılı duraklatmalar geri alınır ve menü eski canlı çizim modunda açılır.

- MENU **SELECT + ÜÇGEN**: Oyun thread'lerini duraklat, menüyü aç; tekrar basınca menüyü kapat ve oyunu devam ettir.
- MENU **O**: Menü kapanır, oyun devam eder.
- **Acil SELECT + START**: Menüde görüntü problemi olsa bile menüyü kapatıp oyunu devam ettirmeyi dener.
- Ses API kancasında menü açıkken oyun çıkış sesi 0 olur; **PSPotify yerel WAV müziği devam eder**.
- `mixer.txt` ve `enabled.txt` kaydetme işlemleri, kullanıcı thread'leri duraklatılmışken kilitli dosya işlemlerini tetiklememek için menü kapanana kadar ertelenir.
- PRX unload edilirse `module_stop` içinde iş parçacıkları geri açılır. **Buna rağmen askıya alma, oyun / CFW kilitlenmesi riski taşır; bütün oyunlarda garantili değildir.**

### Duraklatmayı acilen devre dışı bırakma

`F:\SEPLUGINS\pspotify_bg\pause-game.txt` dosyasının içine tek karakter `0` yaz. Bu dosya mevcut değilse bu deneysel feature branch'te gerçek duraklatma **varsayılan açık**. Önceki davranışa dönmek için PSP'yi yeniden başlat. Daha kapsamlı geri alma için `F:\SEPLUGINS\PLUGINS.TXT` içindeki sadece PSPotify BG `game` satırını `off` yapıp tamamen kapat/aç, çalışan yedek PRX'i geri yükle.

### Kontrollü test

(1) Yeni PRX'i `make -C background clean && python3 scripts/build_background.py` ile derle ve eski PRX'i yedekle. (2) PSP'de kaydın önemli olmadığı bir oyunu **oynanabilir hâle geldikten sonra** aç. (3) SELECT+ÜÇGEN ile menüyü aç: arka planda sahne ve oyun sesi durmalı, müzik WAV devam etmeli; karakter hareket etmemeli. (4) Menüde müzik sesini değiştir ve sonraki WAV'a geç. (5) O ile menüyü kapat: oyun kaldığı yerden devam etmeli, görüntü normale dönmeli. (6) Menüyü ikinci kez aç ve SELECT+START ile acil çıkışı dene. **İlk testte oyun yükleme/UMD video aşamasında duraklatmayı deneme.**

Gerçek cihazda test edilmediği için, derleme başarılı olsa bile bu kodun sorunsuz çalışacağı henüz söylenemez. `master` değişmedi. PSPotify'ın Spotify akışı hâlâ GAME PRX'inde değil; yalnızca mevcut yerel WAV çalar devam eder.

## BG-10 — Oyun tuşlarının menüye hapsedilmesi ve sahne geçişinde menü (25 Ekim 2026)

**Son gerçek cihaz raporu:** Önceki menü düzeltmesi sonrası normal oyun sahnesinde menü sabit, müzik/oyun sesleri bağımsız ayarlanıyor, önceki/sonraki parça çalışıyor. **Açık sorunlar:** Menü tuşları oyuna da gidiyordu; oyun sahnesi değiştiğinde menü gidip geliyordu. Bunlar yeni koddan önce raporlandı; **aşağıdaki düzeltmeler henüz PSP'de denenmedi.**

**Yeni giriş yakalama:** `background/bg_input.c` içinde GAME ortamındaki `sceCtrlReadBufferPositive`, `sceCtrlPeekBufferPositive`, negatif eşdeğerleri ve `sceCtrlRead/PeekLatch` için ARK-5 syscall yönlendirmeleri var. Menüyü kontrol eden PRX worker gerçek tuşları okumaya devam eder; menü açıkken oyun `Buttons=0`, analog ortada olacak şekilde nötr giriş görür. SELECT+ÜÇGEN açılış kombinasyonu da oyuna verilmemeye çalışılır. Menüyü kapatırken hâlâ basılı tuşlar tümü serbest bırakılana kadar gizlenir. Oyun kendi kontrolcü API çağrılarını bypass ederse bu önlem yeterli olmayabilir. `status.log` içinde `BG: controller capture installed for game menu` veya `WARNING controller capture unavailable` aranmalı. Diğer controller PRX'leriyle hook çakışması olabilir.

**Yeni görüntüleme:** `background/bg_overlay.c` içinde yeni kare `sceDisplaySetFrameBuf` kancasından gelince çizim yapılmaya devam eder; ayrıca **her vblank'te aktif VRAM görüntüsü yeniden boyanır**. Sahne geçişlerinde karelerin sonradan menüyü silmesini azaltmaya yöneliktir. RAM içindeki veya bilinmeyen framebuffer bölgelerine yazmaz; her oyunda kesin kesintisiz çizim garantisi yok.

**Yalnızca PRX değişir.** PSPotify `EBOOT.PBP`, eski WAV'lar ve mevcut `enabled.txt`/`mixer.txt` değişmeden kalır. `master` değiştirilmedi.

**Test sırası:** (1) PRX'i temiz derle, (2) eski PRX'i yedekle ve yenisini kopyala, (3) PSP'yi tamamen yeniden başlatıp aynı oyunu aç, (4) SELECT+ÜÇGEN ile menüyü aç, yukarı/aşağı/sol/sağ/X kombinasyonlarını kullan; **oyundaki karakter/kamera/menü hiç hareket etmemeli**, (5) menüyü O ile kapat, **oyun kontrolü normal çalışmalı**, (6) oyun içi sahne/menü değiştirerek overlay'in görünür kalmasını 30 saniye izle, (7) oyun ve müzik sesinin önceki gibi ayrı değiştiğini doğrula. Bir donma veya devamlı kontrol engeli olursa BG plugin satırını `off` yap ve tamamen yeniden başlat; `pspotify_bg.prx.pre-menu.bak` yedeğine dön.

## BG-09 — Titreyen menüye yönelik senkron görüntüleme düzeltmesi

**Cihaz bulgusu (25 Ekim 2026):** Oyun sırasında `SELECT+ÜÇGEN` menüsü açıldı; ancak sürekli titriyor / görünür-kaybolur hâlde. Eski kod her 8,5 ms'de o anda ekranda bulunan tek framebuffer'a çiziyordu. Oyunun çift tamponlu çizimi menünün üstüne yeni kare getirdiği için bu yaklaşım doğal olarak kararsızdı.

**Yeni kod (PSP üzerinde tekrar test edilecek):**

- `sceDisplaySetFrameBuf` PSP NID `0x289D82FE`, ARK `sctrlHENFindFunction` ile keşfedilir; `sctrlHENPatchSyscall` ile kullanıcı çağrısı ekran değiştirmeden **önce** oyunun göstereceği yeni tampona menü çizilir.
- Hook olmayan veya kanca çağrısı gelmeyen oyunlarda eski rasgele ~8,5ms iki kez çizim kaldırıldı; **vblank** ile eşzamanlı tek çizim yedeği kullanılır. Bu yedek bazı oyunlarda hâlâ titreyebilir.
- Oyun dışındaki RAM adresleri boyanmaz; yalnız PSP'nin 2 MiB eDRAM penceresindeki 16/32 bit framebuffer adresleri kabul edilir. Grafik yazımı satır bazında optimize edildi.
- PRX kapanırken display hook geri alınır; açık kaldığına dair varsayım yapılmaz.
- `status.log`: `BG: display flip hook installed; synchronized overlay` veya `BG: no display hook, vblank polling fallback`. İlki **kancanın kurulduğunu** gösterir; oyunun gerçekten o yolu çağırdığını ancak menünün cihaz testi belirler.

**Yalnız PRX güncelle:** Bu düzeltmede PSPotify `EBOOT.PBP`, `enabled.txt`, `mixer.txt`, `config.txt`, WAV ve `PLUGINS.TXT` dosyaları değişmez. Önceki çalışan PRX'i `pspotify_bg.prx.pre-menu.bak` adıyla yedekte tut; güncel `dist/background/pspotify_bg.prx` dosyasını yükle. Oyunu açıp yalnız menüyü aç/kapat ve menü titremesini kontrol et. Donma olursa BG satırını `off` yaparak yeniden başlat; eski PRX'i geri yükle.

**Test kriteri:** Menü en az 30 saniye açıkken oyunun görüntüsü yenilenmesine rağmen görünür kalması, tuşların tepki vermesi, menüyü kapatınca oyunun normal devam etmesi. `master` bu test geçene kadar değiştirilmez.

## Yeni oyun içi mini menü (deneysel — PSP doğrulaması bekleniyor)

PSP oyunu içindeyken **SELECT + ÜÇGEN (△)** ile VSH tarzı siyah/yeşil PSPotify BG menüsünü aç/kapat. Menü GAME PRX'inde çalışır; XMB `vsh` ve PS1 `pops` ortamlarına kurulmaz.

| Kontrol | Görev |
| --- | --- |
| SELECT + △ | Oyun içi menüyü aç/kapat |
| ↑ / ↓ | Menüde satır seç |
| ← / → | Seçili müzik/oyun sesini %10 değiştir veya müziği aç/kapat |
| X | Aç/kapat, sonraki, önceki veya duraklat işlevini çalıştır |
| O | Menüyü kapat |

Menü satırları: **MUSIC** (ON/OFF), **MUSIC VOL** (0–100%), **GAME VOL** (0–100%), **NEXT TRACK**, **PREV TRACK**, **PAUSE / PLAY**.

**Oyun sesini ayrı kısmak için** ARK-5 `SystemCtrlForKernel` kancası kullanılıyor ve PSP'nin bilinen `sceAudioOutput`, `sceAudioOutputBlocking`, panned, SRC ve Audio2 syscall girişlerinde oyunun ses parametreleri oransal azaltılıyor. Bu hook'lar önceki, doğrulanmış WAV PRX sürümünden farklı olarak **yeni ve donma riski olan** deneysel kod. Varsayılan GAME VOL **100%** (değişiklik yok). GAME VOL satırında `GAME AUDIO HOOK UNAVAILABLE` yazarsa CFW'de kanca kurulamamıştır; sağ/sol hiçbir değişiklik yapmaz. Bazı oyunların doğrudan ses sürücülerini veya desteklenmeyen çıkış yollarını kullanması nedeniyle **bütün sesler kısılmayabilir**. **Spotify BG müzik çıkışı** kendi thread'i üzerinden hook'tan muaf tutulur; müzik sesi ayrı ayarlanır.

`MUSIC` seçimi, PSPotify'deki aç/kapat ayarıyla aynı `enabled.txt` dosyasına yazılır. `mixer.txt` dosyası ayrı iki ses düzeyini kaydeder ve sonraki oyunda hatırlar. Menü oyun sırasında `MUSIC=OFF` olsa bile açılabilir; müzik kanalı kapatılır fakat küçük kontrol thread'i açık kalır. Oyun içindeki değişiklik PSPotify yeniden açıldığında da ayarda görüntülenir.

Oyun tuşları menü tarafından engellenmez; oyun aynı tuş girişlerini de görebilir. Bir oyunda SELECT+△ çakışırsa kullanımı durdur ve bildir. Menü ekranı mevcut framebuffer'a yazılarak çizilir; oyun 16-bit/32-bit veya alışılmadık görüntü güncelleme yolu kullanıyorsa menü titreyebilir, hatta görünmeyebilir. Bu aşamada **flicker/uyumluluk garantisi verilmiyor**.

### Yeni PRX'i kurmadan önce (kritik)

**Önce çalışan eski dosyaların yedeğini al.** PSP'de USB sürücüsü F: varsayımı:

```powershell
Copy-Item "F:\SEPLUGINS\pspotify_bg\pspotify_bg.prx" "F:\SEPLUGINS\pspotify_bg\pspotify_bg.prx.pre-menu.bak" -Force
Copy-Item "F:\PSP\GAME\CAT_Araclar\PSPOTIFY\EBOOT.PBP" "F:\PSP\GAME\CAT_Araclar\PSPOTIFY\EBOOT.PBP.pre-menu.bak" -Force
```

Mevcut `PLUGINS.TXT` satırları ve `category_lite.prx` korunur. EBOOT değişimi yalnızca yeni Ayarlar sayfası ve oyun içi aç/kapat eşitlemesi içindir. `master` üzerinde birleştirme **yapılmadı**.

**İlk test sırası:** (1) PRX ve yeni EBOOT temiz derleme, (2) müzik Kapalıyken oyuna gir → SELECT+△ menüsü, (3) menüden MUSIC Açık → WAV çalıyor, (4) MUSIC VOL 70→30 ayarı ile yalnızca müzik kısılıyor, (5) GAME VOL 100→50→0 ile oyun sesi kısılıyor ancak müzik çalıyor, (6) ileri/geri/duraklat, (7) menüyü kapatıp XMB'ye dön ve PSPotify aç → aç/kapat durumu korunmuş.

**Acil geri alma:** Ses kesilirse veya oyun donarsa `F:\SEPLUGINS\PLUGINS.TXT` içindeki yalnızca PSPotify BG `game, ...` satırını `off` yap; PSP'yi tamamen kapat/aç. Ardından `pspotify_bg.prx.pre-menu.bak` dosyasını eski adına geri kopyalayabilirsin. XMB'den açılan normal PSPotify uygulaması PRX'den bağımsızdır ve temiz `master` EBOOT yedeğine döndürülebilir.

**Kapsam sınırı:** Bu PRX şu anda oyun içinde **yerel WAV dosyaları** çalar ve onların önceki/sonraki/parça sesini kontrol eder. **Spotify'ın gerçek akışını oyun sırasında oynatmaz veya Spotify kuyruğunu yönetmez.** Wi-Fi/Ogg/Spotify oturum devamı sonraki, ayrı teknik aşamadır.

## Yeni özellikler: PSPotify ayarından aç/kapat ve oyun içi şarkı değiştirme

**Henüz gerçek Spotify akışı yok.** Bu geliştirme, oynatma durumunu ve tuş kontrollerini gerçek PSP'de doğrulamak için yerel WAV örneklerini kullanıyor. PSPotify uygulamasında **Ayarlar → Oynatma → Oyun müziği (WAV testi)** adında yeni bir **Kapalı / Açık** tercihi var. Varsayılan **Kapalı**.

- **Kapalı:** `ms0:/SEPLUGINS/pspotify_bg/enabled.txt` dosyasına `0` yazılır. PRX, ARK tarafından yüklense bile ses başlatmaz.
- **Açık:** Aynı dosyaya `1` yazılır. PRX yeni açılan PSP oyunlarında ses başlatmayı dener.
- **PSPotify uygulamasının içinde:** PRX kendi test müziğini PSPotify'nin Spotify müziğine karıştırmamaya çalışır; uygulama modülünü tanıyınca durur.
- Ayar `settings.ini` ile saklanır. Başka bir oyundayken PSPotify ayarlarına erişilemiyor; değişiklik oyun açılmadan önce yapılmalı.

Oyunda (PRX sürümü güncellendikten ve üç örnek WAV dosyası kopyalandıktan sonra):

| Kombinasyon | İşlev |
| --- | --- |
| **SELECT + R** | Sonraki parça |
| **SELECT + L** | Önceki parça |
| **SELECT + Kare (□)** | Duraklat / devam ettir |

Kombinasyonlar oyun tuşlarının kullanımını engellemez; oyun aynı basışları görebilir. Önce ana menüsü olmayan sakin bir oyun sahnesinde dene. 3 test parçası sırasıyla `music01.wav`, `music02.wav`, `music03.wav` dosyalarına yüklenir (en fazla sekiz: `music01.wav`–`music08.wav`). Hiç numaralı WAV yoksa eski `music.wav` kullanılır. İleri/geri şimdilik yalnızca bu WAV parçalarını değiştirir, **Spotify kuyruğunu değiştirmez**.

### Yeni prototipi güncelleme

1. `git pull --ff-only origin feature/background-audio-prx` komutundan sonra **ana PSPotify EBOOT'unu da** (normal `scripts/build.py` ile) yeniden derle. Çünkü Açık/Kapalı ayarı EBOOT'a eklendi. PSPotify EBOOT'u `F:\PSP\GAME\CAT_Araclar\PSPOTIFY\EBOOT.PBP` yoluna kopyala (kendi uygulama konumun farklıysa onu kullan).
2. `scripts/build_background.py` ile ayrı PRX'i de derle; `dist/background/pspotify_bg.prx` ve `music01.wav`–`music03.wav` dosyalarını PSP'deki `F:\SEPLUGINS\pspotify_bg\` klasörüne kopyala. Önceki `config.txt` ve `music.wav` dosyasını değiştirmek zorunda değilsin.
3. PSPotify'yi başlatıp Ayarlar → Oynatma → **Oyun müziği (WAV testi) → Açık** seç. Uygulamadan çık, ardından oyunu aç.
4. İleri/geri, duraklat ve ayar **Kapalı** durumunu ayrı ayrı test et. Yeni EBOOT yüklenmezse `enabled.txt` oluşturulmaz, PRX güvenli biçimde **sessiz** kalır.

**Dikkat:** Kurulum tamamsa fakat müzik gelmiyorsa `enabled.txt` dosyasının ilk karakterinin `1` olduğunu, `status.log` dosyasını ve `music01.wav` dosyasının varlığını kontrol et. `game` eklentisi ARK'de açık olması sesin otomatik etkin olduğu anlamına gelmiyor.

## 1. Kaynağı çek ve sadece PRX'i derle

PowerShell:

```powershell
cd D:\Projects\PSPotify-ME
git fetch origin
git switch feature/background-audio-prx
git pull --ff-only origin feature/background-audio-prx

docker run --rm --mount "type=bind,source=D:\Projects\PSPotify-ME,target=/workspace" -w /workspace pspdev/pspdev:latest sh -c "apk add --no-cache python3 && python3 scripts/build_background.py"
```

Derleme başarılıysa `BACKGROUND PRX BUILT (not PSP-tested)` yazısı ve `dist/background/` klasöründe şu dosyalar oluşur:

- `pspotify_bg.prx` — oyunda çalışan ayrı ses eklentisi.
- `music.wav` — yaklaşık 10 saniyelik, **bu projede sıfırdan üretilmiş** örnek melodi; otomatik döngü.
- `config.txt` — `music_volume=70` (0–100), `loop=1` (1: tekrar, 0: tek sefer).

Spotify uygulamasını yeniden derlemene veya EBOOT'u değiştirip kopyalamana gerek yok. Sadece PRX dosyasını test ediyoruz.

## 2. PSP Memory Stick'e kopyala

USB bağlı, Memory Stick sürücüsünün **F:** olduğunu varsayarak PowerShell:

```powershell
New-Item -ItemType Directory "F:\SEPLUGINS\pspotify_bg" -Force | Out-Null
Copy-Item "D:\Projects\PSPotify-ME\dist\background\pspotify_bg.prx" "F:\SEPLUGINS\pspotify_bg\pspotify_bg.prx" -Force
Copy-Item "D:\Projects\PSPotify-ME\dist\background\music.wav" "F:\SEPLUGINS\pspotify_bg\music.wav" -Force
Copy-Item "D:\Projects\PSPotify-ME\dist\background\config.txt" "F:\SEPLUGINS\pspotify_bg\config.txt" -Force
```

**ARK-5 eklentisi:** `F:\SEPLUGINS\PLUGINS.TXT` zaten varsa **üzerine yazma**. Var olan satırların **en altına** aşağıdaki **tek satırı** ekle; önce mevcut dosyanın yedeğini al:

```text
game, ms0:/SEPLUGINS/pspotify_bg/pspotify_bg.prx, on
```

ARK-5'te eklenti yöneticisinden etkinliğini de kontrol edebilirsin. Bu `game` satırı **oyunların tamamına** uygulanabilir. İlk denemeyi önemli bir oyun kaydının bulunmadığı, sorunsuz açılan basit bir PSP oyununda yap. Normal PSP oyununu test et; PS1/POPS başka bir runlevel'dır. Eklentiyi `always` veya `vsh` olarak yükleme.

USB'den ayrılınca eklentinin algılanması için PSP'yi **tam yeniden başlat** (standby'dan çıkarmak yerine). Oyun başladığında eklenti otomatik olarak `music.wav` dosyasını döndürmeye çalışır.

## 3. Hemen yapılacak yalnızca 3 test

| Test | Adım | PASS |
| --- | --- | --- |
| BG-01 | Normal bir PSP oyununu aç. Oyuna dön, sonra XMB'ye çık. | Oyun ve XMB donmadan çalışıyor; eklenti modülü yüklenmiş. **İlk yükleme gözlemi başarılı; XMB'ye dönüş ayrıca doğrulanacak.** |
| BG-02 | Oyunda ~10 saniyelik müziğin tekrar ettiğini dinle. | **PASS (ilk cihaz denemesi):** WAV sesi duyuldu; ses kalitesine ilişkin açık kontrol var. Tekrar/döngü ve oyun sesiyle birlikte çalma ayrıca doğrulanacak. |
| BG-03 | Oyunun ayarlarından **oyun müziğini/ses efektlerini 0 yapabiliyorsan** kapat; `config.txt` içindeki `music_volume` değerini değiştirip tekrar dene. | Oyunun sesi susarken arka plan melodisi duyuluyor; PRX müzik sesi ayrı değişiyor |

**BG-03 otomatik oyun sessize alma değildir.** Bu ilk PRX oyunun sistem ses fonksiyonlarını hook'lamıyor; bu yüzden oyun sesini yalnızca kendi oyun ayarından kapatabilirsin. Dinamik oyun sesi kısmayı güvenle sağlayan bir audio mixer/hook tasarlamak için **önce BG-01/02 PASS** görmemiz gerek. Eski MP3PlayerPlugin'in ses syscall yamasında donma raporları bulunuyor.

`F:\SEPLUGINS\pspotify_bg\status.log` oluşmalıdır. İçinde şunlardan biri olacaktır:

- `BG: module loaded (GAME)` — yükleyici çalıştı.
- `BG: audio playback started` — WAV tanındı ve ses kanalı alındı.
- `BG: no audio channel` — oyun uygun ses kanalı bırakmadı.
- `BG: invalid WAV` — dosya PSP için uygun formatta değil.
- `BG: audio output failed` — sürücüden çıkış alınamadı.

Oyun açılıyor ama ses gelmiyorsa **önce status.log'u paylaş**. Hata kaydı bu aşamada çok değerli.

## 4. Kendi müziğini kullan

Spotify akışı henüz PRX'e bağlanmadı. Sahip olduğun/lisanslı bir ses dosyasını Windows'ta ffmpeg ile PSP'nin yerel formatına çevirebilirsin:

```powershell
ffmpeg -i "D:\Music\ornek.mp3" -ac 2 -ar 44100 -c:a pcm_s16le "D:\Music\music.wav"
```

Oluşan dosyayı `F:\SEPLUGINS\pspotify_bg\music.wav` olarak değiştir. WAV/PCM büyük olabilir (yaklaşık 10 MB/dakika); PRX dosyayı RAM'e bir defada almadan küçük parçalardan okur. Desteklenen: **RIFF WAV, PCM16, 44100Hz, mono/stereo**. MP3/AAC/Ogg şimdilik doğrudan desteklenmiyor.

## 5. Anında geri alma

Eklenti yanlış çalışırsa oyunu tekrar tekrar denemek zorunda değilsin:

1. PSP USB bağlantısıyla `F:\SEPLUGINS\PLUGINS.TXT` dosyasındaki **yalnızca bu satırın** sonundaki `on` ifadesini `off` yap veya satırı kaldır.
2. PSP'yi tamamen yeniden başlat.
3. Gerekirse `F:\SEPLUGINS\pspotify_bg\` klasörünü kaldır. Mevcut PSPotify `EBOOT.PBP` etkilenmez.

PSP normal açılmıyorsa CFW'nin recovery/eklenti devre dışı yöntemi kullanılarak ilgili `GAME` satırı kapatılmalı; bunun ARK-5'teki menü yolu sürüme göre değişebilir.

## 6. Sınırlar ve sonraki teknik iş

- **Mevcut durum Spotify değil.** Test parçası yerel WAV. Wi-Fi, Spotify oturumu ve Ogg Vorbis decoder **PRX'e bağlanmadı**.
- Mevcut `spotify/player.c` kullanılıyor denemez; PRX bağımsız ve küçük.
- Oyun başlarken EBOOT işlemleri sona erdiğinden şarkı devrinin kesintisiz olması henüz mümkün değil; ses/oturum el değiştirme protokolü tasarlanacak.
- Bir boş PCM kanalı veya SRC kanalı yoksa ses oynatımı başlamaz; oyun normal devam etsin diye eklenti başarısızlığı yumuşak karşılar.
- Bazı oyunlar ses kanalını sonradan açabilir, SRC kanalını kullanabilir veya ses aygıtı üzerinde sahiplik bekleyebilir; çakışma testi oyuna göre yapılacak.
- **Oyun sesini otomatik kısan sistem çapı hook bu sürümde yok.** Bunu cihazdaki sonuçlara ve geri alma yoluna göre ayrı bir deneyde değerlendireceğiz.
- Oyun içinde sesi manipüle etmek için sistem çağrılarını yamalamak yeni hata ve çökme riski getirir; önce güvenli ses üretimini kanıtlayacağız.

İlgili araştırma ve kaynaklar: [BACKGROUND_AUDIO_RESEARCH_TR.md](../docs/BACKGROUND_AUDIO_RESEARCH_TR.md).
