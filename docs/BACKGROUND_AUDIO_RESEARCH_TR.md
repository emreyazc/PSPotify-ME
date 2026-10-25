# PSPotify ME — Background Playback / Oyun Sırasında Arka Planda Çalma

**Durum (25 Ekim 2026):** Bağımsız GAME PRX **PSP-3000 / 6.61 ARK-5 üzerinde oyun sırasında WAV sesi üretti**. Böylece oyun içi eklenti yüklemesi ve PCM çıkışının ilk cihaz denemesi başarılı. Üretilmiş deneme melodisinin kalitesi kötü bulundu; gerçek kayıtla karşılaştırma henüz yapılmadı. **Oyunun kendi sesi ile PRX müziği eşzamanlı duyuldu (MIX-01 PASS); sessize alma gerekli olmadığı ilk cihaz testinde görüldü.** Temiz çıkış, çoklu oyun ve ses düzeyi ayarı testleri henüz sonuçlanmadı. Spotify akışı ve otomatik oyun sesini kısma mevcut değil. Kurulum: [background/README_TR.md](../background/README_TR.md).

**Hedef cihaz:** PSP-3000, sistem yazılımı 6.61, ARK-5.
**Ayrı geliştirme dalı:** `feature/background-audio-prx`.
**Temel ilke:** Kullanıcının PSP'de test ederek onayladığı `master` EBOOT sürümü **hiçbir şekilde bozulmayacak**. Mevcut Spotify, temalar ve ayarlar aynı çalışmaya devam etmeli.

## Teknik bulgular

1. **Oyun içi ses üreten PRX örneği var.** MusicPRX (irsmp3 türevi) gerçek PSP oyunları sırasında MP3 çalabiliyor. Ancak bildirilen cihaz testi PSP-3000 + **6.60 LME**; bu, 6.61 ARK-5 ve Spotify için başarı kanıtı **değil**.
   - Bilgi: https://www.gamebrew.org/wiki/MusicPRX_PSP
   - İncelenebilecek kaynak: https://github.com/PSP-Archive/MP3PlayerPlugin (GPL-3.0; doğrudan kod alacaksak lisans incelemesi gerekli).
2. **ARK-5 GAME PRX eklentilerini yükleyebiliyor.** ARK'nin eklenti yönetimi `SEPLUGINS/PLUGINS.TXT` ve GAME / oyun kimliği runlevel desteğini içeriyor. Örnek (yalnızca gelecekteki deney için, **şimdi eklemeyin**):
   ```text
   game, ms0:/seplugins/pspotify_bg.prx, on
   ```
   - ARK-5 kaynak: https://github.com/PSP-Arkfive/ARK-5/blob/main/XMBControl/src/plugins.c
   - ARK eklenti belgesi (ARK-4'ten devreden uyumluluk bilgisi): https://github.com/PSP-Archive/ARK-4/wiki/Plugins
3. **Mevcut PSPotify tek başına oyun arasında yaşamaz.** `main.c` bir kullanıcı uygulaması (`PSP_MODULE_INFO("PSPotify ME", 0, ...)`). `spotify/ui.c`, `spotify/player.c` ve `spotify/session.c` içindeki UI/player/fetch/keepalive thread'leri bu uygulamaya ait. Oyuna geçişte EBOOT yaşam döngüsü ve bellek alanı değişir. Oyunda ayrı PRX hizmeti başlatılmadan müziğin sürmesi beklenemez.
4. **Mevcut ses/decoder pahalı ve çakışmaya açık.** `spotify/player.c` `sceAudioSRCOutputBlocking()`, ~3 MiB ağ ring'i, ~384 KiB Vorbis belleği ve Media Engine task kullanıyor; `main.c` Media Engine imajı/firmware doğrulaması ve hassas temizleme işlemleri yürütüyor. Oyunlar ses kanalları, ME/AVCODEC, WLAN, thread ve kullanıcı RAM'i kullanabilir. EBOOT'taki mevcut ME bootstrap kodunu izinsiz game plugin içinde çalıştırmak güvenli kabul edilemez.
5. **XMB arka plan müziği başka bir problem.** `vshbgm` yalnızca XMB/VSH'ta çalışıyor ve oyun açılırken müziği kapatıyor; bu iş için doğru referans MusicPRX'in oyun içi çalışması. https://github.com/PSP-Archive/vshbgm
6. **Bazı oyunların kendi müzik özelliği var.** PES 2014 ve bazı başka oyunlar dosyadan özel müzik seçebiliyor; bu **Spotify akışı / oyunlar arası evrensel PRX** hedefimizi karşılamaz. https://www.ppsspp.org/docs/reference/custom-background-music/

## Önerilen mimari

```text
   PSPotify ME EBOOT (mevcut, doğrulanmış uygulama)
      |  [ileride] seçili şarkı / devam bilgisi / güvenli aktarım
      v
   PSPotify GAME PRX (yalnızca açıkça etkinleştirildiğinde)
      |-- Başlat/durdur, bellek ölçümü, hata günlüğü, güvenli çıkış
      |-- Önce küçük bağımsız PCM tonu / dosyadan PCM
      |-- Daha sonra bağımsız ağ bağlantısı ve Spotify oturumu
      |-- Daha sonra Ogg Vorbis decoder ve ses çıkışı
      |-- En son oyun içi tuş kısayolları ve oynatma kontrolleri
      v
   GAME: oyun + PRX birlikte
```

**Önemli sınır:** İlk çözüm oyuna geçişte **kesintisiz** ses garantilemez. Mevcut EBOOT kapanıp GAME PRX yüklendiğinde oturum/ses yeniden başlamak zorunda kalabilir. Gerçek kesintisizlik ayrı bir kalıcı servis, çok daha yüksek ayrıcalık ve ileri düzey geçiş yönetimi gerektirir; temel PRX deneyi kanıtlanmadan bunu vaat etmiyoruz.

## Aşamalı ve geri alınabilir deneyler

| Aşama | Deney | Başarı kriteri | Sınır |
| --- | --- | --- | --- |
| BG-00 | `master` EBOOT'u bozmayan ayrı, **varsayılan kapalı** PRX hedefi ve geri alma yönergesi | PRX dosyası üretilebilir; kurulum yalnızca elle | Oyun sesine/Spotify'a müdahale yok |
| BG-01 | GAME eklentisi giriş/çıkış ve günlüğü | Seçili oyunda modül yüklenir, hata üretmeden kapanır; XMB geri gelir | Firmware/oyun başına test |
| BG-02 | Ağ olmadan küçük PCM tonu veya lisanslı kısa PCM dosyası | Oyun açıkken ses duyulur; oyun donmaz; oyun sesi bozulmaz | **İlk büyük fizibilite kapısı** |
| BG-03 | Oyun içinde güvenli WLAN ve HTTPS isteği | Oyun ağı kullanmasa bile bağlantı kurulabilir; hata halinde oyun etkilenmez | Oyun/CFW ağ çakışmaları |
| BG-04 | Spotify oturum yenileme, parça bilgisi, akış | Parça talebi ve indirme başarılı; token güvenliği korunur | Yeni bir güvenli oturum yaşam döngüsü |
| BG-05 | Ogg Vorbis -> PCM çıkış, düşük bellekli ring | 1 tam şarkı oyunda çalabilir; ses kopması kaynak kullanımı ölçülür | ME/AVCODEC ve CPU çakışması; önce ana CPU çözme düşünülebilir |
| BG-06 | EBOOT'tan durumu devretme, kumandalar, ileri/geri | Oynatma devam bilgisi ve tuşlar çalışır; oyun tuşlarını çalmaz | İlk etapta kesintisiz geçiş zorunlu değil |
| BG-07 | Birden fazla oyunda uzun süreli test | Çökme/bozulma yok; çıkış ve yeniden başlatma güvenli | Oyuna özel kara liste/ayar gerekebilir |

## Güvenlik ve kalite kapıları

- İlk denemelerde **PSP'nin flash0/flash1 alanları değiştirilmeyecek**; CFW güncellenmeyecek, kalıcı sistem patch'i yapılmayacak.
- PRX kendi ayrı kaynak ağacı ve build hedefinde olacak; normal `EBOOT.PBP` kurulum akışı değişmeyecek.
- **Geniş `always` veya `vsh` kurulumu yok**. İlk çalıştırma yalnızca GAME / tercihen tek bir oyun için elle yapılmalı.
- Loglama ve kapatma desteklenecek. PRX yüklenmezse veya başlatılamazsa oyun normal açılabilmeli; karttan PRX'i devre dışı bırakma adımı yazılmalı.
- Çakışan `sceAudioSRC` kanalları için alternatif audio channel / mix stratejisi laboratuvarda araştırılacak; ilk sürümde sistem ses API'sini hook'lamak veya oyun sesini zorla kapatmak yok.
- `sceKernel` / ME patch/boot değişiklikleri ve Media Engine erişimi **oyun testleri olmadan** kopyalanmayacak. Oyunla bellek/priority çakışması ölçülecek.
- Önce **yerel ses**, ardından **WLAN**, en son **Spotify**. Spotify erişim token'ları veya `spotify.cfg` hiçbir şekilde repoya/CI loglarına yazılmayacak.
- Başka projelerden kod taşınırsa lisans uyumluluğu ve kredi ayrı kontrol edilir. MusicPRX GPL ve vshbgm GPL kodunu doğrudan almak yerine sadece mimari davranışlarından faydalanmak daha güvenli.
- **ARK-5 + PSP-3000 üzerinde doğrulanmamış** hiçbir adım belgede PASS sayılmayacak.

## İlk uygulama kararı

İlk kod değişikliği tamamlandı: `background/` altında **yerel WAV müzik oynatan bağımsız GAME PRX** kaynağı ve `scripts/build_background.py` oluşturuldu. Bu prototip yalnızca `BG-00/01/02/03` (PRX yükleme, kısa müzik, ayrı müzik ses ayarı/oyun içinden mute) sınaması içindir; otomatik oyun sesi hook'u ya da Spotify entegrasyonu **yok**. İlk test paketi derlenip PSP'de doğrulanmadan Spotify/TLS/Media Engine karmaşıklığı eklenmeyecek.

**Araştırma tarihi:** 25 Ekim 2026. **Mevcut durum:** İlk PRX kodu derlendi ve oyun sırasında PSP-3000 üzerinde WAV sesi duyuldu. Oyun sesi karışımı / ses kalitesi / uzun süreli kararlılık / gerçek Spotify oynatımı henüz doğrulanmadı.


## BG-04 — Spotify arayüzü ile oyun PRX'i arasında aç/kapat ve parça kontrolü (25 Ekim)

**Kodlandı; henüz yeni sürüm cihazda denenmedi:**

- `spotify/prefs.h/.c`: `PREF_GAME_AUDIO`, `game_music=off/on`; `Playback / Oynatma` altında yeni **Game music (WAV test) / Oyun müziği (WAV testi)** tercihi.
- `spotify/ui.c`: Bu tercih değiştiğinde `ms0:/SEPLUGINS/pspotify_bg/enabled.txt` dosyasına `0` veya `1` yazar; uygulama açılışında da durumu eşitler. Hata halinde açık görünmesini engeller.
- `background/pspotify_bg.c`: Eksik `enabled.txt` kapalı sayılır, PSPotify uygulamasının kendisi açılırsa WAV müziğini araya sokmamaya çalışır; oyun içinde **SELECT+R** sonraki, **SELECT+L** önceki WAV parçası, **SELECT+Kare** duraklat/devam.
- PRX en fazla sekiz numaralı `music01.wav`–`music08.wav` arasındaki mevcut dosyaları sırayla oynatır; hiçbiri yoksa geriye dönük uyum için `music.wav` kullanır.
- `scripts/build_background.py`: Üç ayırt edilebilir, orijinal telifsiz test melodisi üretir.
- `background/README_TR.md`: EBOOT ve PRX çift güncelleme, cihaz testleri ve kurtarma talimatları.

**Önemli:** Yeni kontrol tuşları şu anda Spotify kuyruğuna erişmez; ses kaynağı yerel WAV'dır. Spotify akışını oyun içinde sağlayabilmek için ayrı çalışma ortamında WLAN/TLS/Ogg Vorbis/kimlik yenileme ve ses sahipliği çalışmalarının gerçek cihazda tamamlanması gerekir. Açık/Kapalı ayarı altyapıyı hazırlar; **bu aşamada Spotify'ın oyun arkasında çaldığını iddia etmiyoruz.** Cihaz sonuçlarına göre PRX ve EBOOT ayrı merge kararı verilecek.


## BG-05 — Oyun içi VSH tarzı menü ve bağımsız ses kontrolü (yeni deney, TEST EDİLMEDİ)

**Kod:** `background/bg_overlay.c` ve `bg_audio.c`, `SystemCtrlForKernel` için `systemctrl_stubs.S`, güncellenmiş `pspotify_bg.c` ve `Makefile`.

- GAME bağlamında **SELECT+ÜÇGEN** menüyü açar ve kapar; MUSIC ON/OFF, MUSIC VOL, GAME VOL, NEXT/PREV TRACK, PAUSE/PLAY.
- Menü oyunun mevcut framebuffer'ına basit 5x7 bitmap karakterlerle çizilir. 565/5551/4444/8888 formatları ve yalnızca 2 MiB VRAM aralığına sınırlandırılır; bilinmeyen belleğe yazmayı reddeder.
- Ayrı çizim thread'i açılır ve `module_stop` sırasında kapatılır. Oyun girişleri yakalanıp engellenmez; tuş çakışması mümkündür.
- Oyun sesinin bağımsız 0–100 düzeyinde değişmesi için PSP CFW ses çıkış syscall'ları gerekli NID bulunursa patch edilir; oyun ses parametreleri kısılır, kendi arka plan müzik thread'i etkilenmez. **Bu deneysel kernel hook, bazı oyunlarda çökme riski taşıyabilir.** Varsayılan GAME VOL=100.
- Menüden GAME/MUSIC VOL değiştirmek `mixer.txt` ile, aç/kapat `enabled.txt` ile kaydedilir; PSPotify arayüzü açılışta durumu okur.
- İlk oyun içi WAV/karma ses deneyinin PASS sonucu, **bu yeni menü/hook sürümü için PASS anlamına gelmez**. Yeni kod henüz temiz derleme ve PSP cihaz doğrulamasından geçmedi. `master` değiştirilmedi.
- Derleme ve **geri alma** yönergesi: [background/README_TR.md](../background/README_TR.md). Gerçek Spotify akışı PRX'e taşınmadı; menüdeki parçalar yerel WAV'dır.
