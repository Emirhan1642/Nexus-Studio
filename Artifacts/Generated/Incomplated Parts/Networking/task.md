# Phase 6: Networking (Replication & RemoteEvent)

- `[x]` **Görev 1: Protobuf Şeması**
  - `[x]` `Engine/Networking/Messages.proto` dosyasını oluştur.
  - `[x]` `CMakeLists.txt` içerisine `protobuf_generate_cpp` tanımını ekle.

- `[x]` **Görev 2: Serializasyon (PacketSerializer)**
  - `[x]` `Engine/Networking/Serialization/PacketSerializer.h` dosyasını oluştur.
  - `[x]` `Engine/Networking/Serialization/PacketSerializer.cpp` dosyasını oluştur.

- `[x]` **Görev 3: DataModel Replikasyonu (ReplicationManager)**
  - `[x]` `ReplicationManager.h/cpp` dosyalarını Protobuf kullanacak şekilde güncelle.

- `[x]` **Görev 4: RemoteEvent Uygulaması**
  - `[x]` `RemoteEvent.h/cpp` içerisindeki `std::cout` olan yerleri gerçek ağ çağrılarıyla değiştir.

- `[x]` **Görev 5: Transport Sınıflarının (Server/Client) Bağlanması**
  - `[x]` `NetworkServer.h/cpp` dosyasını gelen mesajları işleyecek şekilde güncelle.
  - `[x]` `NetworkClient.h/cpp` dosyasını gelen mesajları işleyecek şekilde güncelle.

- `[x]` **Görev 6: Derleme ve Test**
  - `[x]` `Tests/NetworkingTests.cpp` dosyası ekle ve RemoteEvent + Replication senaryolarını sına.
  - `[x]` CMake yapılandırmasını derle ve çalıştırıldığından emin ol.
