- `[x]` **Görev 1: ClientConnection Güncellemesi**
  - `[x]` `NetworkServer.h` içerisinde `ClientConnection` struct'ına `InstanceId playerCharacter` alanını ekle.

- `[x]` **Görev 2: SpatialGrid ve Gerçek Konum Güncellemeleri**
  - `[x]` `InstanceRegistry` sınıfına tüm objeleri dolaşmak için (veya çekmek için) bir iteratör/getter ekle.
  - `[x]` `ReplicationManager::flushToAllClients` içinde tüm `Instance`'ların `Position` özelliklerini okuyarak `SpatialGrid`'i güncelle.

- `[x]` **Görev 3: Relevancy & Oyuncu Merkezli İlgi Alanı**
  - `[x]` `ReplicationManager::flushToAllClients` metodunda `playerPos` değişkenini, o anki kullanıcının `playerCharacter` pozisyonundan oku.
  - `[x]` Gerçek Hysteresis mesafe hesaplamalarını (`RelevancyTracker.h` ve döngüdeki distance) karakter ile obje pozisyonu arasına kur.

- `[x]` **Görev 4: Derleme ve Test**
  - `[x]` `NetworkingTests.cpp` dosyasında bir Part oluşturup `playerCharacter` olarak bağlayarak Spatial Culling'i doğrula.
  - `[x]` Gerekirse derleme hatalarını gider ve bir Culling testi yaz.
