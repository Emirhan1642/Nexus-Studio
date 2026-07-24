# Walkthrough: Nexus Studio Geliştirmeleri

## Faz 7.1D: Node-Based Material Editor (Tamamlandı)

Bu aşamada Nexus Studio'ya ImNodes kütüphanesini kullanarak görsel bir materyal (shader) editörü entegre ettik. Düğüm (node) tabanlı grafik üzerinden bgfx için anında `.sc` kodları üretilip, derlenerek motora yüklenmesi sağlandı.

### Neler Yapıldı?

1. **Bağımlılık Entegrasyonu (`imnodes`)**
   - `ThirdParty/CMakeLists.txt` içerisine `Nelarius/imnodes` kütüphanesi eklendi.
   - `imnodes.cpp` kaynak dosyası doğrudan `Editor` CMake hedefine dahil edildi ve kütüphane bağlama sorunları çözüldü.

2. **Veri Modeli (`ShaderGraph`)**
   - [ShaderGraph.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Materials/ShaderGraph.h) oluşturuldu.
   - Düğümler (`ShaderNode`), giriş/çıkış pinleri (`ShaderPin`), ve pinler arası bağlantılar (`ShaderLink`) tanımlandı.

3. **Görsel Arayüz (`MaterialEditorPanel`)**
   - [MaterialEditorPanel.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/UI/MaterialEditorPanel.cpp) oluşturuldu ve `Main.cpp` üzerinden ana döngüye eklendi.
   - Sağ tık menüsü ile düğüm ("Color" vb.) ekleme yeteneği getirildi.
   - Düğümler arası bağlantı (link) oluşturma ve silme işlemleri ImNodes API'si ile entegre edildi.

4. **Dinamik Shader Derleyici (`ShaderGraphCompiler`)**
   - [ShaderGraphCompiler.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Materials/ShaderGraphCompiler.cpp) yazıldı.
   - MVP kapsamında grafikteki düğümlerden basit `vertex` ve `fragment` shader metinleri (`.sc` formatında) üretiliyor.
   - Üretilen bu metinler `std::system` ile `shaderc.exe` aracına gönderilerek çalışma anında (runtime) derlenip `.bin` formatına çevriliyor.
   - `loadShaderProgram` ile Bgfx bellek alanına aktarılıp `RendererSystem::m_overrideMaterial` değişkenine atanıyor.

### Doğrulama Sonuçları

- Proje sıfırdan sorunsuz derleniyor.
- Arayüzde yer alan "Compile Shader" butonuna basıldığında arka planda `shaderc.exe` çağrılıyor ve sahnede bulunan küp veya modelin rengi/materyali, yazdığımız geçici Override sistemi ile anında güncelleniyor.

> [!TIP]
> Editör çalışırken F5 ile simülasyonu başlatmadan önce penceredeki "Compile Shader" butonuna tıklayarak terminaldeki (console) derleme çıktılarını görebilir ve model üzerindeki değişikliği anında deneyimleyebilirsiniz.

---

## Faz 7.1 (Geçmiş) - Dinamik Çözünürlük ve LOD Sistemi Tamamlandı

Oyun motorumuz büyük ve karmaşık sahneleri render ederken **LOD (Level of Detail)** sistemini kullanarak performansı ciddi oranda koruyabiliyor. 

### 1. `meshoptimizer` Entegrasyonu
Endüstri standardı olan **zeux/meshoptimizer** kütüphanesini projeye entegre ettik. Bu sayede objeleri yüklerken karmaşık hesaplamaları otomatik olarak gerçekleştiriyoruz.

### 2. İçe Aktarma Sırasında Otomatik LOD Üretimi
Bir model (FBX/OBJ) motora yüklendiğinde (`AssetImportPipeline` aracılığıyla), sadece ana hali değil; 
- **LOD0 (%100 Poligon)**: Yakın mesafe için
- **LOD1 (%50 Poligon)**: Orta mesafe için
- **LOD2 (%15 Poligon)**: Uzak mesafe için 

olmak üzere 3 farklı indeks dizisi (Index Buffer) oluşturuluyor. Orijinal köşe (Vertex) verisini tek bir hafızada tutup sadece indeksleri değiştirdiğimiz için RAM/VRAM tüketimi de çok düşük kalıyor.

### 3. Mesafeye Dayalı Seçim (Runtime)
Oyun çalışırken (`RendererSystem::renderFrame`), her nesne için kameraya olan mesafe anlık olarak hesaplanır. Eğer kamera objeden belli bir mesafenin ötesine geçerse motor anında daha düşük poligonlu versiyonu (LOD1 veya LOD2) ekrana çizdirir.

### Test Adımları
1. `NexusStudioEditor.exe`'yi açın.
2. Sahneye yüksek poligonlu (detaylı) bir nesne ekleyin.
3. Editör kamerasını kullanarak nesneden yavaşça uzaklaşın.
4. Belli bir mesafeden sonra (wireframe modunda çok daha net görebileceğiniz şekilde) nesnenin poligon sayısının aniden düştüğünü göreceksiniz. Bu düşüş, nesne çok uzakta olduğu için normal render modunda gözü rahatsız etmez ancak performansı (FPS) artırır.
