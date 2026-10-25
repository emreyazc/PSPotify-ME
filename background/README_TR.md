# PSPotify BG — PSP'de oyun içi müzik PRX prototipi

**Durum (25 Ekim 2026, yeni sürüm geliştirme aşamasında):** PSP-3000 / 6.61 ARK-5 üzerinde bir PSP oyunu çalışırken **deneme WAV sesi duyuldu**: GAME PRX yüklenmesi ve yerel ses çıkışı doğrulandı. Kullanıcı ilk test melodisinin sesini kalitesiz buldu; bunun üretilmiş örnek ses mi yoksa ses çıkışı kaynaklı mı olduğu henüz ayrıştırılmadı. **Oyunun kendi sesiyle PRX müziğinin aynı anda duyulduğu doğrulandı (MIX-01 PASS).** XMB'ye temiz dönüş, çoklu oyun ve bağımsız müzik ses ayarı henüz ayrıca doğrulanmadı. **Spotify akışı hâlâ PRX'e bağlı değil.**

Bu paketi kullanırken mevcut PSPotify ME `EBOOT.PBP` dosyasına **dokunmuyorsun**. PSP hafızasının `flash0` / `flash1` alanlarına **hiçbir şey yazılmaz**. Eklenti yalnızca bilinçli olarak `PLUGINS.TXT` dosyasına eklenince yüklenir.

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
