# PSPotify ME — Özellikler ve Kullanım Kılavuzu (Türkçe)

> Bu belge `feature/list-cover-toggle` geliştirme dalındaki uygulamayı anlatır. PSP üzerinde yapılacak Arşiv Görünümü ve SELECT kısayolu testleri tamamlanmadan bu sürümü doğrulanmış bir cihaz sürümü olarak değerlendirmeyin. İngilizce karşılığı: [FEATURES_EN.md](FEATURES_EN.md).

## Müzik arşivi ve gezinme

- **Beğenilen Şarkılar:** Spotify kütüphanesindeki beğenilen parçaları açar ve oynatır.
- **Listeler:** Spotify çalma listelerini görüntüler. Sıralama seçenekleri: Spotify sırası, ada göre veya en çok şarkı içeren önce. Üçgen menüsü üzerinden liste sabitleme / sabitlemeyi kaldırma mümkündür.
- **Benim Listelerim:** Kullanıcıya ait listeler ayrı kategoridedir. İsim sırasıyla listelenir; sabitlenen listeler önceliklidir.
- **Liste içi:** Listeye girildiğinde şarkılar gösterilir; seçili şarkı oynatılır. Liste içinde gezinirken oynatma kuyruğu kendi bağlamını korur.
- **Arama:** Kare tuşuyla Spotify'da arama başlatılır.
- **Son Dinlenenler:** Gerçek şarkı geçmişini son dinlenenden başlayarak gösterir; tekrar ziyaret edildiğinde yenileme yapılır. Spotify tarafındaki değişiklikler gecikmeli gelebilir.
- **Sıradakiler/Kuyruk:** Yaklaşan şarkıları görüntüler, şarkı ekleyip çıkarma ve SELECT ile yer değiştirme desteklenir.

## Liste biçimi ve görseller

`Ayarlar > Görünüm > Liste biçimi`:

| Seçenek | İşlev |
|---|---|
| Rahat | Daha büyük, kapaklı standart satırlar |
| Sıkışık | Daha küçük standart satırlar |
| Yalnızca yazı | Tek sütunda kapaksız metin satırları |
| **Arşiv Görünümü** | **İki sütunda sayfa başına en fazla 20 öğe; liste kapağı yok** |

**Arşiv Görünümü**, Beğenilen Şarkılar, **Son Dinlenenler**, Listeler, Benim Listelerim ve bir listenin içindeki şarkılar için ortaktır. Görünen sıra numaraları 1–10 solda, 11–20 sağdadır; sonraki 20 öğeye seçim ilerledikçe geçilir. Yukarı/aşağı tuşları **sıradaki/önceki öğeyi** seçer; soldan sağ sütuna geçmek için sol/sağ tuşları yeniden atanmaz. **Sol/sağ tuşları kategori değiştirmeye devam eder.** Liste içindeki son satır/son öğe, otomatik sonraki 20'lik sayfaya geçer.

Seçili öğe mavi/vurgu renkli arka planla işaretlenir. Alttaki detay şeridi, şarkılarda ad/sanatçı/süre/sıra-toplam; listelerde ad/sahip (veri varsa)/şarkı sayısı/sıra-toplam gösterir. **Spotify'dan gelen mevcut metadata'da albüm adı yer almadığı için** albüm adı bu detay şeridinde gösterilmez; olmayan tarih/alanlar uydurulmaz. Listenin içindeyken üstte liste adı gösterilir.

`Ayarlar > Görünüm > Liste görselleri` ayarı Açık/Kapalıdır ve `list_covers = on/off` olarak saklanır. Kapatıldığında küçük görsel indirmeleri durur, ilgili doku önbelleği temizlenir ve mini oynatıcı kapaksız olur. **Arşiv Görünümü bu ayardan bağımsız olarak kapaksızdır:** ayar satırı açıklamayla etkisizleşir; mevcut Açık/Kapalı tercihi, normal liste biçimine dönüldüğünde korunur. Arşiv modunda bulanık albüm kapağı arka planı da gezinme ekranlarında çizilmez. Tema duvar kâğıdı bağımsızdır. **Şimdi Çalıyor** ekranındaki büyük kapak normal şekilde çalışır (Şimdi Çalıyor düzeni kullanıcı tarafından *Yalnızca yazı* seçilmemişse).

Arama, Kuyruk ve Ayarlar, mevcut XMB satır yerleşimini korur. Arşiv modu etkinken bu ekranlarda da küçük liste kapakları indirilmez; iki sütun tasarımı yukarıdaki **beş** bölümde uygulanır.

## SELECT ile Şimdi Çalıyor ↔ kaldığın liste

Müzik çalıyorken Beğenilen Şarkılar, Listeler, Benim Listelerim, liste içi, Arama ve Son Dinlenenler gibi gezinme ekranlarında **SELECT** tuşuna basınca **Şimdi Çalıyor** açılır. **SELECT** tekrar basıldığında daha önceki kategoriye ve aynı liste seviyesine geri dönülür. Çalan parça **dönülen listenin görünür/yüklenmiş kayıtlarında bulunuyorsa**, seçim o an gerçekten çalan parçaya kaydırılır (L/R ile değiştirilen parça da takip edilir). Oynatılıyor simgesi 7. şarkıdaysa imleç de 7. şarkıya gelir. Çalan şarkı listede bulunmuyorsa **SELECT öncesindeki imleç konumu** korunur; kuyruğun sıra numarasına veya eski çalma kaynağı anahtarına güvenilmez. Son Dinlenenler'den SELECT ile dönüşte dinleme geçmişi gereksiz yere yeniden yüklenerek seçim sıfırlanmaz.

- Çalan bir şarkı yoksa hızlı geçiş tetiklenmez.
- **Kuyruk** ve **Ayarlar > Kategoriler** ekranında SELECT'in mevcut öğe taşıma işlevi her zaman önceliklidir; bu işlev **değiştirilmedi**.
- **START** uygulamadan çıkar; SELECT yerine kullanılmaz.
- Kısayol **tüm liste biçimlerinde** geçerlidir; Arşiv Görünümü'ne özel değildir.
- "Son Dinlenenler" Spotify dinleme geçmişidir; **"Şimdi Çalıyor" ayrı bir ekrandır**.

## Mevcut kumandalar

| Tuş | Gezinme/listeler | Şimdi Çalıyor |
|---|---|---|
| ↑ / ↓ | Önceki / sonraki öğe | Ses |
| ← / → | Önceki / sonraki kategori | Kategori değiştirme |
| ✕ | Aç / oynat | Tekrar modu |
| ○ | Açık listenin üst seviyesine dön | — |
| □ | Oynat / duraklat; Arama'da arama yaz | Oynat / duraklat |
| △ | Seçenekler; üst düzey listede sabitleme | Karıştırma |
| L / R | Şarkı atla veya basılı tutup sar (ayarlarla özelleştirilebilir) | Aynı |
| SELECT | Şimdi Çalıyor / listeye dön; özel taşıma listelerinde mevcut taşıma | Kaldığın listeye dön |
| START | Uygulamadan güvenli çıkış | Aynı |

Onay tuşu (`✕`/`○`) ayarlardan değiştirilebilir; uygulamadaki onay/geri davranışı bu tercihe göre çalışır.

## Şarkılar, sıralama ve yükleme

- `Ayarlar > Yerleşim > Şarkıları sırala`: **En yeni önce (varsayılan), En eski önce, Başlık A-Z, Sanatçı A-Z**. Şarkılar için `Spotify sırası` seçeneği kaldırılmıştır; bu seçenek **çalma listelerinin** sıralamasında hâlâ vardır.
- `Ayarlar > Yerleşim > Yükleme başına şarkı`: **25 / 50 / 75 / 100**. Listenin sonuna yaklaşıldığında sonraki parti istenir.
- Liste görünümü, sıralaması veya sayfalama değiştirilince gerçek Spotify çalma listesi sunucuda yeniden sıralanmaz.
- Bu sürümde tek bir şarkı listesi için kullanılan üst veri sınırı **500 parçadır**; Arşiv Görünümü bu sınırı kaldırmaz.

## Çalma özellikleri ve ses

Ogg Vorbis akışı PSP üzerinde çalınır; Media Engine ses çözer. Oynat/duraklat, sonraki/önceki parça, ileri/geri sarma, karıştırma, tekrar, kuyruk düzenleme, beğenilenlere ekleme/çıkarma, ses normalizasyonu, ekolayzır ve beş bantlı özel ekolayzır desteklenir. Şimdi Çalıyor ekranında Klasik/Ortalanmış/Yalnızca yazı düzenleri, isteğe bağlı görselleştirici, sanatçı ve şarkı bilgileri bulunur. `Kapak renkleri` aktifken çalınan parçanın kapağı tema rengini etkileyebilir.

## Ağ, ayarlar, kişiselleştirme

- PSP'de kayıtlı Wi-Fi profilleri denenir; biri bağlantı/IP alamadığında sıradakine geçilir. PSP'nin uyumlu olduğu kablosuz güvenlik özellikleri geçerlidir.
- Spotify Premium ve ilk açılışta Spotify eşleştirmesi gereklidir.
- Tema, duvar kâğıdı, arka plan, kapak biçimi, yazı boyutu, liste görünümü, animasyon, saat, tuş ipuçları, Türkçe/İngilizce dili, başlangıç ekranı, kategori düzenleme/gizleme, ses ve oynatma ayarları mevcuttur.
- Pil tasarrufu, uyku zamanlayıcısı, L/R davranışı, onay tuşu ve kulaklık kumandası ayarlanabilir.
- Ayarlar `settings.ini` içinde saklanır. Yeni anahtar `list_style = archive` seçildiğinde Arşiv Görünümü, `list_covers = off` seçildiğinde liste kapakları kapalıdır.
- Spotify Connect desteği bu dalda bitmiş/kabul edilmiş bir özellik olarak değerlendirilmemelidir.

## Teknik notlar ve durum

PSP'nin gerçek ekranı **480 × 272** pikseldir. Arşiv tasarımının iki sütun × on satır hedefi bu alana sıkı tipografiyle sığdırılır. İlk PSP testinde yeşil seçili satırın dışına taşan yazılar görüldü; satır yazısı gerçek satır yüksekliğine göre küçültülüp dikey ortalanacak şekilde düzeltildi. Uzun isimler sütun genişliğine göre kısaltılır. 24 Ekim PSP testinde Son Dinlenenler arşivi ve yeşil seçim kutusu hizası **PASS** aldı; SELECT dönüşünde güncel parçaya odaklanmama **FAIL** verdi. Bu son soruna yönelik ek yama henüz gerçek PSP'de tekrar doğrulanmamıştır. Teknik mimari: [IMPLEMENTATION.md](IMPLEMENTATION.md). Kurulum ve hata çözme: [SETUP.md](SETUP.md).
