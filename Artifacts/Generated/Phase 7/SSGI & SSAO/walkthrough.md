# Faz 7.1 (MVP) - SSGI & SSAO Eklentisi Tamamlandı!

Post-Processing hattımıza **SSGI (Screen Space Global Illumination)** ve **SSAO (Screen Space Ambient Occlusion)** efektlerini başarıyla ekledik.

## Neler Değişti?

### 1. MRT (Çoklu Çizim Hedefi) Altyapısı
Motorumuzun "Forward Rendering" aşaması artık her piksali sadece "Renk" ve "Derinlik" olarak değil, aynı zamanda **Normal** (Yüzeyin baktığı yön) olarak da ara belleğe (G-Buffer) kaydediyor.

### 2. SSAO (Ortam Gölgelemesi)
Işığın ulaşmakta zorlandığı köşeler, çatlaklar ve birbirine yakın objelerin temas noktaları artık kararıyor. Bu sayede sahnede objelerin "uçuyormuş" gibi görünmesi engellendi ve gerçekçilik büyük oranda arttı.

### 3. SSGI (Ekran Uzayı Dolaylı Aydınlatma)
Ekranda bir yüzeye vuran ışık, o yüzeyin rengini alarak etrafındaki diğer yüzeylere yansıyor (Color Bleeding). Şimdilik MVP seviyesinde `fs_ssgi.sc` içerisinde 8 örnekleme (ray-marching) ile birleştirilmiş bir Noise (Gürültü) filtresi üzerinden çalışıyor.

## Nasıl Test Edeceksiniz?
1. `NexusStudioEditor.exe`'yi açın.
2. Sahneye birbirine çok yakın 2 obje veya köşeli bir geometri ekleyin. (Örneğin bir kutu ve hemen yanına bir zemin).
3. Objelerin birleştiği köşelerde doğal bir gölge (SSAO) oluştuğunu göreceksiniz.
4. Işık alan parlak renkli (örneğin kırmızı) bir objeyi, gri veya beyaz bir duvara yaklaştırdığınızda duvarın hafifçe o rengi (SSGI) aldığını gözlemleyebilirsiniz.

Şu anki SSGI/SSAO MVP seviyesinde olduğundan ufak kumlanmalar (Noise) görülebilir. Daha sonra bu kısımlar temporal (zaman bağımlı) filtrelerle veya daha yüksek çözünürlüklü blur ile geliştirilebilir.

Testlerinizi yaptıktan sonra bir sonraki hedefe geçebiliriz! (Örn. LOD Sistemi)
