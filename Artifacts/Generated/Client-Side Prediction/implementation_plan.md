# Phase 6: Client-Side Prediction (Networking) Implementation Plan

Bu plan, `HumanoidPredictor` ve `ClientPredictor` sınıflarında yarım bırakılan Client-Side Prediction (İstemci Tarafı Tahmini ve Sunucu Uzlaşmazlığı) altyapısının eksiksiz bir şekilde tamamlanmasını amaçlamaktadır.

## User Review Required

> [!IMPORTANT]
> Bu aşamada Protobuf mesaj tanımları değiştirilecek ve Ağ İstemcisi/Sunucusu döngülerine komut işleme mekanizmaları eklenecektir. `HumanoidPredictor` istemcide yerel olarak oluşturulup `NetworkClient`'a kaydedilecek. Mimari yaklaşımın onaylanması gerekmektedir.

## Open Questions

- `NetworkClient` şu an `HumanoidPredictor`'a doğrudan erişemiyor. `HumanoidPredictor` nesnesini `NetworkClient` içerisinde `localPredictor` olarak saklamayı ve gelen Snapshot'ları doğrudan ona yönlendirmeyi planlıyorum. Bu yaklaşım uygun mudur?
- Sunucu tarafında `ClientConnection` struct'ı `playerCharacter` barındırıyor. Sunucu gelen komut paketini bu karakterin `Humanoid` bileşenine `applyMovement()` üzerinden mi uygulamalıdır?

## Proposed Changes

### Protobuf Updates

#### [MODIFY] [Messages.proto](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Messages.proto)
- `PlayerInputPacket` eklenecek: `sequence_number`, `delta_time`, `move_direction`, `jump_requested`.
- `PlayerStateSnapshotPacket` eklenecek: `sequence_number`, `position`, `velocity`.
- `NetworkPacket` payload'una bu iki yeni mesaj `oneof` field'ı olarak eklenecek.

### Networking Serialization

#### [MODIFY] [PacketSerializer.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Serialization/PacketSerializer.h)
#### [MODIFY] [PacketSerializer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Serialization/PacketSerializer.cpp)
- `buildPlayerInputPacket` ve `buildPlayerStateSnapshotPacket` metodları eklenecek.

### Core Prediction Logic

#### [MODIFY] [HumanoidPredictor.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Prediction/HumanoidPredictor.h)
#### [MODIFY] [HumanoidPredictor.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Prediction/HumanoidPredictor.cpp)
- `sendToServer` metodu, `NetworkClient::send()` ve `PacketSerializer` kullanacak şekilde implemente edilecek.

### Network Transport

#### [MODIFY] [NetworkClient.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkClient.h)
#### [MODIFY] [NetworkClient.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkClient.cpp)
- `setLocalPredictor(std::shared_ptr<HumanoidPredictor>)` metodu eklenecek.
- `poll()` içerisinde gelen `PlayerStateSnapshotPacket` ayrıştırılıp `localPredictor->onServerSnapshot()` metoduna yönlendirilecek.

#### [MODIFY] [NetworkServer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkServer.cpp)
- `poll()` içerisinde gelen `PlayerInputPacket` ayrıştırılacak.
- İlgili `ClientConnection` üzerinden karakterin `Humanoid` bileşeni bulunup `applyMovement()` (veya input'a bağlı zıplama) uygulanacak.
- Sunucunun bu simülasyon sonrasındaki karakter pozisyonu ve hızı alınarak, `PlayerStateSnapshotPacket` oluşturulup istemciye geri gönderilecek.

## Verification Plan

### Automated Tests
- `NetworkingTests.cpp` içerisine `ClientSidePredictionTest` eklenecek:
  - İstemciden sahte bir hareket girdisi oluşturulup sunucuya yollanması simüle edilecek.
  - Sunucunun snapshot döndürmesi ve istemcide "pending commands" (bekleyen komutlar) temizlenerek reconciliation işleminin gerçekleştiği doğrulanacak.
