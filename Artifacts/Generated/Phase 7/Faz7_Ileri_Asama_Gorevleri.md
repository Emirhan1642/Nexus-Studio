# Faz 7.2 - 7.4 İleri Seviye Grafik Görevleri (Roadmap)

Aşağıdaki liste, Faz 7.1 (MVP) sonrasında motorun "Unreal kalitesine" ulaşması için gereken gelişmiş grafik mimarisi eksiklerini içermektedir.

## Faz 7.2 Hedefleri
- `[x]` **Görev 1: Voxel Cone Tracing (VCT) Tabanlı Global Illumination**
  - Sahnenin çalışma anında voxel ızgaraya (3D Texture) çevrilmesi (Voxelization Pass).
  - Voxel verisi üzerinden Mipmap üretilmesi.
  - PBR shader içerisinde Cone Tracing ile dolaylı aydınlatmanın hesaplanması.
- `[x]` **Görev 2: Çoklu Cascade Gölgeler (Cascaded Shadow Maps - CSM)**
  - Mevcut tek kaskatlı gölgenin, mesafeye göre 3 parçaya bölünmesi.
  - Frustum hesaplamaları ve projeksiyon matrislerinin kaskatlara göre güncellenmesi.
- `[x]` **Görev 3: FXAA (Fast Approximate Anti-Aliasing)**
  - Post-Processing zincirinin sonuna kenar yumuşatma shader'ının eklenmesi.

## Faz 7.3 Hedefleri
- `[x]` **Görev 4: Dithered LOD Transitions (Yumuşak LOD Geçişleri)**
  - LOD seviyeleri değişirken (Örn: LOD0 -> LOD1) objenin bir anlık yok olup belirmesini (pop-in) engellemek için piksel bazlı erime efektinin eklenmesi.
- `[x]` **Görev 5: Motion Blur & Depth of Field (DOF)**
  - Post-Process pipeline'ına hareket bulanıklığı ve alan derinliği efektlerinin entegrasyonu.
- `[x]` **Görev 6: Contact Shadows**
  - Ekran uzayı derinlik haritası (Depth Buffer) kullanılarak çok küçük detayların gölgelerinin belirginleştirilmesi.

## Faz 7.4+ Hedefleri
- `[x]` **Görev 7: SSR (Screen Space Reflections)**
  - Yansıtıcı yüzeylerde ekran uzayı verisini kullanarak gerçek zamanlı çevresel yansıma hesaplanması.
- `[x]` **Görev 8: Temporal Anti-Aliasing (TAA) ve Temporal Gölge Filtreleme**
  - Önceki karelerin (frames) verisini kullanarak titremeleri (flickering) önleyen geçici filtreleme algoritmalarının entegrasyonu.
