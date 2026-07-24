# Hedef: Faz 6 Ağ (Networking) Eksiklerinin Tamamlanması

Kullanıcının dikkat çektiği üzere, `HumanoidPredictor` (Client-Side Prediction) mekanizması eklenmiş olsa da, sunucu-istemci mimarisinin omurgasını oluşturan **Veri (DataModel) Replikasyonu** ve **RemoteEvent** iletişim sistemi şu an sadece kod taslağı (Mock / `std::cout`) halinde. 

Bu plan, **Faz 6 (Networking)** gereksinimlerini %100 tamamlamak için uygulanacak adımları listeler.

## User Review Required
> [!IMPORTANT]
> Bu aşamada, verileri serialize/deserialize etmek için **Protocol Buffers (Protobuf)** kullanacağız (halihazırda `ThirdParty/CMakeLists.txt` içerisine ekli). Protobuf sayesinde property'leri küçük bytelar halinde ağ üzerinden güvenle yollayabileceğiz. Planı onaylarsanız tüm bu sistemi aktif hale getireceğim.

## Açık Sorular (Open Questions)
> [!WARNING]
> Şimdilik sadece tek oyunculu bir projeyi multiplayer'a çeviriyoruz. İleride RemoteEvent için argüman tiplerini filtrelemek veya sınırlandırmak (güvenlik açısından) istenecek mi? Şimdilik temel Protobuf tipleri (int, float, string, Vector3) üzerinden destek ekleyeceğim.

## Önerilen Değişiklikler

---

### Protobuf Şemaları (Schema)
Paket yapılarını tanımlamak için Protobuf `.proto` dosyası oluşturulacak.

#### [NEW] [Messages.proto](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Messages.proto)
- `PropertyData`: Property türüne göre veriyi barındıracak (string, float, int, vector3 vb.)
- `ReplicationPacket`: Hangi Instance'ın hangi özelliklerinin (Property) değiştiğini tutacak.
- `RemoteEventPacket`: Çağrılan RemoteEvent'in adını ve argüman listesini tutacak.

---

### Serializasyon Katmanı (Serialization)
DataModel'den gelen reflection verisini Protobuf formatına (Byte array) dönüştürecek katman.

#### [NEW] [PacketSerializer.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Serialization/PacketSerializer.h)
#### [NEW] [PacketSerializer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Serialization/PacketSerializer.cpp)
- `buildReplicationPacket(Instance, dirtyProperties)`: Değişen özellikleri reflection üzerinden çekip `Messages.pb.h` formatına dönüştürecek.
- `applyReplicationPacket(Instance, Packet)`: Gelen paketi reflection üzerinden yerel DataModel'e uygulayacak.

---

### Ağ Transport & Replikasyon (Transport & Replication)
Mevcut Mock edilmiş yapılar gerçek GNS ve Protobuf ile değiştirilecek.

#### [MODIFY] [ReplicationManager.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Replication/ReplicationManager.cpp)
- `flushToAllClients` içindeki "Mock" yorum satırları silinecek, yerine `PacketSerializer` ile paketler Protobuf'a çevrilip `NetworkServer::sendTo()` kullanılarak byte stream olarak ağa gönderilecek.

#### [MODIFY] [RemoteEvent.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/RemoteEvent.cpp)
- `std::cout` olan mock kodları silinip, `PacketSerializer` ile RemoteEventPacket (Protobuf) oluşturulacak. İlgili `NetChannel::Reliable_Ordered` kanalıyla sunucuya/istemciye aktarılacak.

#### [MODIFY] [NetworkClient.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkClient.cpp)
#### [MODIFY] [NetworkServer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkServer.cpp)
- `poll()` içerisinde ağdan gelen bytelar Protobuf'a `ParseFromArray` ile geri çevrilecek ve ilgili `RemoteEvent` tetiklenecek veya `ReplicationManager` aracılığıyla DataModel güncellenecek.
- `CMakeLists.txt` içerisinde `protobuf_generate_cpp` kullanılarak bu sınıfların derlenmesi sağlanacak.

## Doğrulama Planı (Verification Plan)

### Otomatik Testler
- `NexusStudioTests.exe` içerisine `NetworkingTests.cpp` eklenecek.
- Sanal bir Sunucu ve İstemci (localhost) ayağa kaldırılarak `RemoteEvent::FireServer` üzerinden veri iletimi test edilecek.
- Sunucuda oluşturulan bir objenin, replikasyonla istemci DataModel'ine geçip geçmediği doğrulanacak.
