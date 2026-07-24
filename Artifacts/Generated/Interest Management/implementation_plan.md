# Faz 6 Devamı: Interest Management Uygulaması

Bu plan, erken MVP aşamasında atlanan ve devasa çok oyunculu (MMO) ölçeklenmesi için gerekli olan "Interest Management" (İlgi Yönetimi) sisteminin tam teşekküllü olarak motora entegre edilmesini kapsar.

## User Review Required

> [!IMPORTANT]
> Bu aşama, sunucunun tüm dünyayı her istemciye göndermesini engelleyerek yalnızca oyuncunun etrafındaki "ilgi alanında" (Interest Zone) bulunan objeleri senkronize etmesini sağlar.
> 
> Oyuncu karakterlerinin sunucu tarafında her bir `ClientConnection` için atanması (assign) gerekecektir. Test için geçici bir "Kamera Pozisyonu" veya "Sahte Oyuncu Karakteri" atayarak sistemi test edeceğiz. Planı onaylarsanız geliştirmeye başlayacağım.

## Proposed Changes

### 1. `Engine/Networking/Transport/NetworkServer.h`
- `ClientConnection` yapısına `InstanceId playerCharacter` eklenecek. Böylece ağ üzerinden bağlı olan bir oyuncunun oyundaki fiziksel karşılığının hangi obje olduğu bilinecek.

### 2. `Engine/Networking/Replication/ReplicationManager.h & cpp`
- `flushToAllClients` içerisindeki `Math::Vector3 playerPos(0.0f, 0.0f, 0.0f);` mock kodu, `ClientConnection::playerCharacter` üzerinden (Reflection kullanılarak `Position` property'si okunarak) gerçek karakter koordinatlarıyla değiştirilecek.
- Dünyadaki tüm fiziksel objelerin (`Part`, `Humanoid` vb.) konumları her *tick* te Reflection kullanılarak okunup `m_spatialGrid.updateInstancePosition` üzerinden güncellenecek.

### 3. `Engine/Networking/Replication/SpatialGrid.h`
- Hâlihazırda bulunan kod, gerçek objelerin eklenmesi ve çıkarılması döngüsüne entegre edilecek. 2 boyutlu (X, Z düzlemi) Cell boyutları 100 stud olarak korunacak.

### 4. `Engine/Networking/Replication/DormancyManager.h` & `RelevancyTracker.h`
- **Hysteresis (RelevancyTracker):** ENTER_RADIUS (örneğin 300) ve EXIT_RADIUS (örneğin 400) eşikleri tam olarak uygulanacak.
- **Dormancy:** Hareketsiz ve özelliği değişmeyen objeler 30 saniye sonra uykuya geçecek (Dormant) ve Spatial Grid taramalarından çıkarılarak CPU tasarrufu sağlanacak.

### 5. `Tests/NetworkingTests.cpp`
- Test ortamında sunucuya sahte bir `Part` oluşturulup `playerCharacter` olarak atanacak.
- Uzaktaki bir objenin (500 stud uzakta) replike edilmediği, yakındaki bir objenin (50 stud) replike edildiği (Hysteresis mantığı) doğrulanacak.

## Verification Plan

### Automated Tests
- `NetworkingTests.cpp` içerisinde "Spatial Grid Culling" (Uzamsal Kırpma) testi çalıştırılacak.

### Manual Verification
- C++ birim testinin log çıktılarında `RelevancyTracker::Action::Create` ve `Destroy` aşamalarının doğru mesafelerde tetiklendiği görülecek.
